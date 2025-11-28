#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/logger.h"
#include <iomanip>

void ShmBase::Create() {
  if (!shm_.Create()) {
    throw std::runtime_error("Failed to create shared memory");
  }
  is_creator_ = true;  // 标记为创建者
  // if (!sem_.Create()) {
  //   throw std::runtime_error("Failed to create semaphore");
  // }
}

void ShmBase::initMutexAndCond() {
  ShmHead* head = static_cast<ShmHead*>(shm_.Data());
  if (!head) {
    throw std::runtime_error("获取共享内存头部指针失败");
  }

  // 3. 初始化互斥锁（进程间共享）
  pthread_mutexattr_t mutex_attr;
  int ret = pthread_mutexattr_init(&mutex_attr);
  if (ret != 0) {
    throw std::runtime_error("初始化互斥锁属性失败：" +
                             std::string(strerror(ret)));
  }
  // 设置互斥锁可跨进程共享
  ret = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    throw std::runtime_error("设置互斥锁进程共享属性失败：" +
                             std::string(strerror(ret)));
  }
  // 初始化互斥锁
  // 注意：如果互斥锁已经初始化，pthread_mutex_init 会返回 EBUSY 或 EINVAL
  // 这种情况理论上不应该发生，因为我们已经检查了 initialized_ 标志
  ret = pthread_mutex_init(&head->mutex_, &mutex_attr);
  if (ret != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    throw std::runtime_error("初始化互斥锁失败：" + std::string(strerror(ret)));
  }
  pthread_mutexattr_destroy(&mutex_attr);

  // 4. 初始化条件变量（进程间共享）
  pthread_condattr_t cond_attr;
  ret = pthread_condattr_init(&cond_attr);
  if (ret != 0) {
    throw std::runtime_error("初始化条件变量属性失败：" +
                             std::string(strerror(ret)));
  }
  ret = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("设置条件变量进程共享属性失败：" +
                             std::string(strerror(ret)));
  }
  // 初始化条件变量（如果已初始化，上面的 mutex_destroy 中已经销毁了）
  ret = pthread_cond_init(&head->cond_, &cond_attr);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("初始化条件变量失败：" +
                             std::string(strerror(ret)));
  }
  pthread_condattr_destroy(&cond_attr);

  // 设置初始化标志
  head->initialized_ = 0x4D525332;  // "MRS2"
  head->time_ = 0;
  head->ref_count_ = 1;  // 创建者初始化为1
  head->data_size_ = data_max_size_;
  head->cur_msg_size_ = 0;
  head->write_pos_ = 0;
  head->read_pos_ = 0;
  head->max_msg_size_ = max_msg_size_;  // 存储消息最大大小
  head->slot_size_ = slot_size_;        // 存储消息槽大小
  CachePointers(head);
}

void ShmBase::CachePointers(ShmHead* head) {
  mutex_ptr_ = &head->mutex_;
  cond_ptr_ = &head->cond_;
  time_ptr_ = &head->time_;
  data_size_ptr_ = &head->data_size_;
  ref_count_ptr_ = &head->ref_count_;
  cur_msg_size_ptr_ = &head->cur_msg_size_;
  write_pos_ptr_ = &head->write_pos_;
  read_pos_ptr_ = &head->read_pos_;
  max_msg_size_ptr_ = &head->max_msg_size_;  // 缓存消息最大大小指针
  slot_size_ptr_ = &head->slot_size_;         // 缓存消息槽大小指针

  // 数据区紧跟在ShmHead之后
  // 使用 sizeof(ShmHead) 计算偏移，确保指向数据区开始
  // ShmData 的 data_[0] 是柔性数组成员，不占用空间
  // 将 head 转换为 ShmData* 时，data_[0] 就指向 ShmHead 之后的第一个字节
  // ShmData *shm_data = reinterpret_cast<ShmData *>(head + 1);
  // data_ptr_ = shm_data->data_;

  // 或者更直接的方法（等价）：
  // data_ptr_ = reinterpret_cast<char *>(head) + sizeof(ShmHead);
  data_ptr_ = reinterpret_cast<char*>(head) + sizeof(ShmHead);
}

bool ShmBase::Exists() const { return shm_.Exists(); }
void ShmBase::Write(const void* data, size_t size, size_t offset) {
  // 检查消息大小是否超过最大限制（使用共享内存中的值）
  size_t max_size = max_msg_size_ptr_ ? *max_msg_size_ptr_ : max_msg_size_;
  if (size > max_size) {
    throw std::out_of_range("消息大小超过最大限制: " + std::to_string(size) + 
                            " > " + std::to_string(max_size));
  }
  
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("互斥锁指针未初始化，请先调用 Open()");
  }
  
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("获取互斥锁失败：" + std::string(strerror(ret)));
  }
  
  try {
    size_t write_pos = *write_pos_ptr_;
    size_t slot_size = slot_size_ptr_ ? *slot_size_ptr_ : slot_size_;
    
    // 检查是否会超出数据区大小（使用固定槽大小）
    if (offset + write_pos + slot_size > data_max_size_) {
      // 根据 QosPolicy 策略处理
      if (qos_policy_.history == QosPolicy::KEEP_LAST) {
        // KEEP_LAST 模式：回绕到开始位置（覆盖旧数据）
        write_pos = 0;
        *write_pos_ptr_ = 0;
      } else {
        // KEEP_ALL 模式：循环队列
        // 如果写指针到达末尾，需要回绕到开始位置
        size_t read_pos = *read_pos_ptr_;
        size_t next_write_pos = write_pos + slot_size;
        
        // 计算回绕后的写位置
        size_t wrapped_write_pos = (next_write_pos >= data_max_size_) ? 
                                   0 : next_write_pos;
        
        // 队列满的判断：回绕后的写位置等于读位置（队列满）
        // read_pos == write_pos 表示队列空，read_pos != write_pos 表示有数据
        // 如果回绕后 read_pos == wrapped_write_pos，说明队列满
        if (wrapped_write_pos == read_pos) {
          // 队列满：直接跳过，不阻塞
          LOGD("队列满：跳过写入，不阻塞");
          pthread_mutex_unlock(mutex_ptr_);
          return;
        }
        
        // 回绕到开始位置
        if (next_write_pos >= data_max_size_) {
          write_pos = 0;
        } else {
          write_pos = next_write_pos;
        }
        *write_pos_ptr_ = write_pos;
      }
    }
    
    char* write_ptr = data_ptr_ + offset + write_pos;  // 写入起始地址
    
    // 先写入 size
    size_t* write_size_ptr = reinterpret_cast<size_t*>(write_ptr);
    *write_size_ptr = size;
    write_ptr += sizeof(size_t);
    
    // 再写入数据（只写入实际大小，剩余空间保留）
    std::memcpy(write_ptr, data, size);
    // 如果数据小于 max_msg_size_，剩余空间保持原样（不清零）
    
    // 更新写指针位置（固定移动 slot_size）
    write_pos += slot_size;
    *write_pos_ptr_ = write_pos;
    
    *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    *data_size_ptr_ = size;
    *cur_msg_size_ptr_ = size;  // 更新当前消息大小
    
    // 通知等待的读者
    pthread_cond_broadcast(cond_ptr_);
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }
  
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("释放互斥锁失败WRITE: " +
                             std::string(strerror(ret)));
  }
}

void ShmBase::WriteUnlocked(const void* data, size_t size, size_t offset) {
  // 检查消息大小是否超过最大限制（使用共享内存中的值）
  size_t max_size = max_msg_size_ptr_ ? *max_msg_size_ptr_ : max_msg_size_;
  if (size > max_size) {
    throw std::out_of_range("消息大小超过最大限制: " + std::to_string(size) + 
                            " > " + std::to_string(max_size));
  }
  
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }
  
  size_t write_pos = *write_pos_ptr_;
  size_t slot_size = slot_size_ptr_ ? *slot_size_ptr_ : slot_size_;
  
  // 检查是否会超出数据区大小（使用固定槽大小）
  if (offset + write_pos + slot_size > data_max_size_) {
    // 根据 QosPolicy 策略处理
    if (qos_policy_.history == QosPolicy::KEEP_LAST) {
        // KEEP_LAST 模式：回绕到开始位置（覆盖旧数据）
        write_pos = 0;
        *write_pos_ptr_ = 0;
    } else {
      // KEEP_ALL 模式：抛出异常
      throw std::out_of_range("写入数据超出共享内存大小: 需要 " + 
                              std::to_string(offset + write_pos + slot_size) + 
                              " 字节，可用 " + std::to_string(data_max_size_) + " 字节");
    }
  }
  
  char* write_ptr = data_ptr_ + offset + write_pos;  // 写入起始地址
  
  try {
    // 先写入 size
    size_t* write_size_ptr = reinterpret_cast<size_t*>(write_ptr);
    *write_size_ptr = size;
    write_ptr += sizeof(size_t);
    
    // 再写入数据（只写入实际大小，剩余空间保留）
    std::memcpy(write_ptr, data, size);
    
    // 更新写指针位置（固定移动 slot_size）
    write_pos += slot_size;
    *write_pos_ptr_ = write_pos;
    
    *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    *data_size_ptr_ = size;
    *cur_msg_size_ptr_ = size;  // 更新当前消息大小
  } catch (...) {
    throw;
  }
}

void ShmBase::ReadUnlocked(void* buffer, size_t size, size_t offset) {
  LOGD("ReadUnlocked");
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }
  
  if (read_pos_ptr_ == nullptr) {
    LOGE("read_pos_ptr_ 为 nullptr，请先调用 Open() 初始化指针");
    throw std::runtime_error("read_pos_ptr_ 未初始化，请先调用 Open()");
  }
  
  try {
    LOGD("ReadUnlocked try");
    size_t read_pos = 0;
    char* read_ptr = nullptr;
    
    size_t slot_size = slot_size_ptr_ ? *slot_size_ptr_ : slot_size_;
    
    if (qos_policy_.history == QosPolicy::KEEP_LAST) {
      LOGD("ReadUnlocked KEEP_LAST");
      // KEEP_LAST 模式：从 write_pos_ 向前一个槽读取最新的消息
      size_t write_pos = *write_pos_ptr_;
      if (write_pos == 0) {
        throw std::runtime_error("没有数据可读（write_pos_ == 0）");
      }
      
      // 计算最新消息的位置（write_pos_ - slot_size，考虑回绕）
      if (write_pos >= slot_size) {
        read_pos = write_pos - slot_size;
      } else {
        // 回绕：从末尾向前
        read_pos = data_max_size_ - slot_size + write_pos;
      }
    } else {
      // KEEP_ALL 模式：从 read_pos_ 读取
      read_pos = *read_pos_ptr_;
      size_t write_pos = *write_pos_ptr_;
      
      // 循环队列判断：read_pos == write_pos 表示队列为空（没有数据可读）
      if (read_pos == write_pos) {
        pthread_mutex_unlock(mutex_ptr_);
        LOGD("没有数据可读read_pos_ == write_pos_队列为空");
        return;
      }
    }
    
    read_ptr = data_ptr_ + offset + read_pos;
    
    // 先读取 size
    size_t* read_size_ptr = reinterpret_cast<size_t*>(read_ptr);
    size_t msg_size = *read_size_ptr;
    *cur_msg_size_ptr_ = msg_size;
    
    if (msg_size > size) {
      throw std::runtime_error("消息大小超过缓冲区: " + std::to_string(msg_size) + 
                              " > " + std::to_string(size));
    }
    
    read_ptr += sizeof(size_t);
    
    // 再读取数据
    std::memcpy(buffer, read_ptr, msg_size);
    
    // 更新读指针位置（仅 KEEP_ALL 模式需要，固定移动 slot_size）
    // 类似于"弹栈"操作：读取后，读指针向前移动，释放该槽位
    LOGD("slot_size: " << slot_size);
    LOGD("qos_policy_.history: " << qos_policy_.history);
    if (qos_policy_.history == QosPolicy::KEEP_ALL) {
      LOGD("ReadUnlocked KEEP_ALL");
      LOGD("read_pos: " << read_pos);
      read_pos += slot_size;
      LOGD("read_pos after: " << read_pos);
      // 处理循环队列：如果读指针到达数据区末尾，回绕到开始
      if (read_pos >= data_max_size_) {
        read_pos = 0;  // 回绕到开始位置
      }
      
      if (read_pos_ptr_ != nullptr) {
        *read_pos_ptr_ = read_pos;  // 更新读指针位置
        // 确保写入立即刷新到共享内存（对于进程间共享内存，通常不需要特殊操作）
        LOGD("更新读指针位置: " << read_pos << " (read_pos_ptr_ = " << read_pos_ptr_ 
             << ", *read_pos_ptr_ = " << *read_pos_ptr_ << ")");
      } else {
        LOGE("read_pos_ptr_ 为 nullptr，无法更新读指针位置");
      }
    }
  } catch (...) {
    throw;
  }
}

void ShmBase::Read(void* buffer, size_t size, size_t offset) {
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("互斥锁指针未初始化，请先调用 Open()");
  }
  
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("获取互斥锁失败：" + std::string(strerror(ret)));
  }
  
  try {
    size_t read_pos = 0;
    char* read_ptr = nullptr;
    
    size_t slot_size = slot_size_ptr_ ? *slot_size_ptr_ : slot_size_;
    
    if (qos_policy_.history == QosPolicy::KEEP_LAST) {
      // KEEP_LAST 模式：从 write_pos_ 向前一个槽读取最新的消息
      size_t write_pos = *write_pos_ptr_;
      if (write_pos == 0) {
        pthread_mutex_unlock(mutex_ptr_);
        throw std::runtime_error("没有数据可读（write_pos_ == 0）");
      }
      
      // 计算最新消息的位置（write_pos_ - slot_size，考虑回绕）
      if (write_pos >= slot_size) {
        read_pos = write_pos - slot_size;
      } else {
        // 回绕：从末尾向前
        read_pos = data_max_size_ - slot_size + write_pos;
      }
    } else {
      // KEEP_ALL 模式：从 read_pos_ 读取，如果没有数据则等待
      read_pos = *read_pos_ptr_;
      size_t write_pos = *write_pos_ptr_;
      
      // 循环队列判断：read_pos == write_pos 表示队列为空（没有数据可读）
      if(read_pos == write_pos) {
        pthread_mutex_unlock(mutex_ptr_);
        LOGE("没有数据可读read_pos_ == write_pos_队列为空");
        return;
        // throw std::runtime_error("没有数据可读（read_pos_ == write_pos_，队列为空）");
      }
    }
    
    read_ptr = data_ptr_ + offset + read_pos;
    
    // 先读取 size
    size_t* read_size_ptr = reinterpret_cast<size_t*>(read_ptr);
    size_t msg_size = *read_size_ptr;
    *cur_msg_size_ptr_ = msg_size;
    
    if (msg_size > size) {
      pthread_mutex_unlock(mutex_ptr_);
      throw std::runtime_error("消息大小超过缓冲区: " + std::to_string(msg_size) + 
                              " > " + std::to_string(size));
    }
    
    read_ptr += sizeof(size_t);
    
    // 再读取数据
    std::memcpy(buffer, read_ptr, msg_size);
    
    // 更新读指针位置（仅 KEEP_ALL 模式需要，固定移动 slot_size）
    // 类似于"弹栈"操作：读取后，读指针向前移动，释放该槽位
    if (qos_policy_.history == QosPolicy::KEEP_ALL) {
      read_pos += slot_size;
      
      // 处理循环队列：如果读指针到达数据区末尾，回绕到开始
      if (read_pos >= data_max_size_) {
        read_pos = 0;  // 回绕到开始位置
      }
      
      if (read_pos_ptr_ != nullptr) {
        *read_pos_ptr_ = read_pos;
        LOGD("Read() 更新读指针位置: " << read_pos);
      } else {
        LOGE("read_pos_ptr_ 为 nullptr，无法更新读指针位置");
      }
      
      // 通知等待的写者（有空间可以写入新消息）
      pthread_cond_broadcast(cond_ptr_);
    }
    
    *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
    
    // 保存实际读取的消息大小（在解锁前）
    
    // 解锁
    ret = pthread_mutex_unlock(mutex_ptr_);
    if (ret != 0) {
      throw std::runtime_error("释放互斥锁失败READ: " +
                               std::string(strerror(ret)));
    }
    
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }
}

void ShmBase::Close() {
  if (!shm_.Close()) {
    throw std::runtime_error("Failed to close shared memory");
  }
}

ShmBase::~ShmBase() {
  bool should_unlink = false;
  
  if (ref_count_ptr_ && mutex_ptr_) {
    // 减少引用计数
    decrementRefCount();
    
    // 检查是否需要 Unlink
    int ret = pthread_mutex_lock(mutex_ptr_);
    if (ret == 0) {
      LOGD("ShmBase destructor: ref_count = " << *ref_count_ptr_);
      if (*ref_count_ptr_ == 0) {
        should_unlink = true;
        LOGD("ShmBase destructor: last reference, will unlink shared memory "
             << name_);
      }
      pthread_mutex_unlock(mutex_ptr_);
    }
  }
  
  // 先关闭映射
  shm_.Close();
  
  // 如果引用计数为0，删除共享内存对象
  if (should_unlink) {
    shm_.Unlink();
  }
}

void ShmBase::incrementRefCount() {
  if (ref_count_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    return;  // 未初始化
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    return;  // 获取锁失败，忽略错误
  }

  (*ref_count_ptr_)++;
  LOGD("ShmBase ref_count incremented to: " << *ref_count_ptr_);

  // 释放锁
  pthread_mutex_unlock(mutex_ptr_);
}

void ShmBase::decrementRefCount() {
  if (ref_count_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    return;  // 未初始化
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    return;  // 获取锁失败，忽略错误
  }

  if (*ref_count_ptr_ > 0) {
    (*ref_count_ptr_)--;
    LOGD("ShmBase ref_count decremented to: " << *ref_count_ptr_);
  }

  // 释放锁
  pthread_mutex_unlock(mutex_ptr_);
}

// 打印指定名称的共享内存内的所有数据
void ShmBase::PrintShmData(const std::string& shm_name, bool hex_dump) {
  try {
    // 打开共享内存
    SharedMemory shm(shm_name);
    if (!shm.Exists()) {
      LOGD("共享内存不存在: " << shm_name);
      return;
    }
    
    shm.Open();
    
    // 读取头部信息
    ShmHead* head = static_cast<ShmHead*>(shm.Data());
    if (!head) {
      LOGE("无法获取共享内存头部指针");
      return;
    }
    
    // 计算数据区指针和大小
    // shm_stat.st_size 是共享内存的实际大小（通过 fstat 获取）
    // 这是 POSIX 共享内存文件的真实大小
    char* data_ptr = reinterpret_cast<char*>(head) + sizeof(ShmHead);
    size_t shm_actual_size = shm.Size();  // 这是通过 fstat 获取的实际大小
    size_t data_max_size = (shm_actual_size > sizeof(ShmHead)) ? 
                          (shm_actual_size - sizeof(ShmHead)) : 0;
    
    // 加锁保护
    int ret = pthread_mutex_lock(&head->mutex_);
    if (ret != 0) {
      LOGE("获取互斥锁失败: " << strerror(ret));
      return;
    }
    
    try {
      LOGD("========== 共享内存信息: " << shm_name << " ==========");
      LOGD("初始化标志: 0x" << std::hex << head->initialized_ << std::dec);
      LOGD("引用计数: " << head->ref_count_);
      LOGD("数据大小: " << head->data_size_ << " 字节");
      LOGD("当前消息大小: " << head->cur_msg_size_ << " 字节");
      LOGD("写指针位置: " << head->write_pos_ << " 字节");
      LOGD("读指针位置: " << head->read_pos_ << " 字节");
      LOGD("共享内存实际大小 (shm_stat.st_size): " << shm_actual_size << " 字节");
      LOGD("ShmHead 大小: " << sizeof(ShmHead) << " 字节");
      LOGD("数据区最大大小: " << data_max_size << " 字节");
      
      // 警告：如果数据区大小为0，说明共享内存创建时没有分配数据区
      if (data_max_size == 0) {
        LOGE("警告：数据区大小为 0！共享内存可能创建时 size 参数为 0 或 history_depth 为 0");
        LOGE("建议：创建共享内存时确保 size > 0 且 history_depth > 0");
      }
      
      LOGD("时间戳: " << head->time_);
      
      LOGD("========== 数据内容 ==========");
      
      if (hex_dump) {
        // 十六进制dump模式
        for (size_t i = 0; i < data_max_size; i += 16) {
          std::cout << std::hex << std::setfill('0') << std::setw(8) << i << ": ";
          for (size_t j = 0; j < 16 && (i + j) < data_max_size; j++) {
            std::cout << std::setw(2) << std::setfill('0') 
                      << static_cast<unsigned int>(static_cast<unsigned char>(data_ptr[i + j])) << " ";
          }
          std::cout << " | ";
          for (size_t j = 0; j < 16 && (i + j) < data_max_size; j++) {
            char c = data_ptr[i + j];
            std::cout << (std::isprint(c) ? c : '.');
          }
          std::cout << std::dec << std::endl;
        }
      } else {
        // 按消息格式打印（先读取size，再读取数据）
        size_t pos = 0;
        int msg_count = 0;
        
        while (pos + sizeof(size_t) <= data_max_size) {
          // 读取消息大小
          size_t* size_ptr = reinterpret_cast<size_t*>(data_ptr + pos);
          size_t msg_size = *size_ptr;
          
          if (msg_size == 0 || msg_size > data_max_size) {
            // 无效消息，跳过
            pos++;
            if (pos >= data_max_size) break;
            continue;
          }
          
          pos += sizeof(size_t);
          
          if (pos + msg_size > data_max_size) {
            // 消息不完整
            break;
          }
          
          msg_count++;
          LOGD("--- 消息 #" << msg_count << " (位置: " << (pos - sizeof(size_t)) 
               << ", 大小: " << msg_size << " 字节) ---");
          
          // 打印消息内容
          char* msg_data = data_ptr + pos;
          
          // 尝试作为字符串打印（如果可打印）
          bool is_printable = true;
          for (size_t i = 0; i < msg_size; i++) {
            if (!std::isprint(static_cast<unsigned char>(msg_data[i])) && 
                msg_data[i] != '\n' && msg_data[i] != '\r' && msg_data[i] != '\t') {
              is_printable = false;
              break;
            }
          }
          
          if (is_printable && msg_size < 1024) {
            // 作为字符串打印
            std::string msg_str(msg_data, msg_size);
            LOGD("内容: " << msg_str);
          } else {
            // 作为十六进制打印
            LOGD("内容 (hex): ");
            for (size_t i = 0; i < msg_size && i < 64; i++) {
              std::cout << std::hex << std::setfill('0') << std::setw(2)
                        << static_cast<unsigned int>(static_cast<unsigned char>(msg_data[i])) << " ";
              if ((i + 1) % 16 == 0) {
                std::cout << std::endl;
              }
            }
            if (msg_size > 64) {
              std::cout << "... (仅显示前64字节)" << std::endl;
            } else {
              std::cout << std::dec << std::endl;
            }
          }
          
          pos += msg_size;
          
          // 如果到达写指针位置，停止
          if (head->write_pos_ > 0 && pos >= head->write_pos_) {
            break;
          }
        }
        
        if (msg_count == 0) {
          LOGD("没有找到有效消息");
        } else {
          LOGD("共找到 " << msg_count << " 条消息");
        }
      }
      
      LOGD("==========================================");
      
    } catch (const std::exception& e) {
      LOGE("读取数据时出错: " << e.what());
    }
    
    pthread_mutex_unlock(&head->mutex_);
    
  } catch (const std::exception& e) {
    LOGE("打印共享内存数据失败: " << e.what());
  }
}

// 通过共享内存名称直接读取数据
size_t ShmBase::ReadData(const std::string& shm_name, void* buffer, size_t buffer_size, 
                         QosPolicy qos_policy) {
  if (buffer == nullptr || buffer_size == 0) {
    throw std::runtime_error("缓冲区指针为空或缓冲区大小为0");
  }
  
  // 打开共享内存
  SharedMemory shm(shm_name);
  if (!shm.Exists()) {
    throw std::runtime_error("共享内存不存在: " + shm_name);
  }
  
  if (!shm.Open()) {
    throw std::runtime_error("打开共享内存失败: " + shm_name);
  }
  
  // 读取头部信息
  ShmHead* head = static_cast<ShmHead*>(shm.Data());
  if (!head) {
    shm.Close();
    throw std::runtime_error("获取共享内存头部指针失败");
  }
  
  // 获取共享内存实际大小
  size_t total_size = shm.Size();
  size_t offset = sizeof(ShmHead);
  size_t data_max_size = (total_size > offset) ? (total_size - offset) : 0;
  
  if (data_max_size == 0) {
    shm.Close();
    throw std::runtime_error("数据区大小为0");
  }
  
  // 从头部读取 slot_size 和 max_msg_size
  size_t slot_size = head->slot_size_;
  size_t max_msg_size = head->max_msg_size_;
  
  if (slot_size == 0 || max_msg_size == 0) {
    shm.Close();
    throw std::runtime_error("共享内存头部信息无效: slot_size=" + 
                            std::to_string(slot_size) + 
                            ", max_msg_size=" + std::to_string(max_msg_size));
  }
  
  // 计算数据区指针
  char* data_ptr = reinterpret_cast<char*>(head) + offset;
  
  // 加锁保护
  int ret = pthread_mutex_lock(&head->mutex_);
  if (ret != 0) {
    shm.Close();
    throw std::runtime_error("获取互斥锁失败: " + std::string(strerror(ret)));
  }
  
  try {
    size_t read_pos = 0;
    char* read_ptr = nullptr;
    
    // 根据 QoS 策略确定读取位置
    if (qos_policy.history == QosPolicy::KEEP_LAST) {
      // KEEP_LAST 模式：从 write_pos_ 向前一个槽读取最新的消息
      size_t write_pos = head->write_pos_;
      if (write_pos == 0) {
        pthread_mutex_unlock(&head->mutex_);
        shm.Close();
        throw std::runtime_error("没有数据可读（write_pos_ == 0）");
      }
      
      // 计算最新消息的位置（write_pos_ - slot_size，考虑回绕）
      if (write_pos >= slot_size) {
        read_pos = write_pos - slot_size;
      } else {
        // 回绕：从末尾向前
        read_pos = data_max_size - slot_size + write_pos;
      }
    } else {
      // KEEP_ALL 模式：从 read_pos_ 读取
      read_pos = head->read_pos_;
      size_t write_pos = head->write_pos_;
      
      // 循环队列判断：read_pos == write_pos 表示队列为空（没有数据可读）
      if (read_pos == write_pos) {
        pthread_mutex_unlock(&head->mutex_);
        shm.Close();
        throw std::runtime_error("没有数据可读（read_pos_ == write_pos_，队列为空）");
      }
    }
    
    read_ptr = data_ptr + read_pos;
    
    // 先读取 size
    size_t* read_size_ptr = reinterpret_cast<size_t*>(read_ptr);
    size_t msg_size = *read_size_ptr;
    
    if (msg_size == 0 || msg_size > max_msg_size) {
      pthread_mutex_unlock(&head->mutex_);
      shm.Close();
      throw std::runtime_error("消息大小无效: " + std::to_string(msg_size));
    }
    
    if (msg_size > buffer_size) {
      pthread_mutex_unlock(&head->mutex_);
      shm.Close();
      throw std::runtime_error("消息大小超过缓冲区: " + std::to_string(msg_size) + 
                              " > " + std::to_string(buffer_size));
    }
    
    read_ptr += sizeof(size_t);
    
    // 再读取数据
    std::memcpy(buffer, read_ptr, msg_size);
    
    // 更新读指针位置（仅 KEEP_ALL 模式需要，固定移动 slot_size）
    if (qos_policy.history == QosPolicy::KEEP_ALL) {
      read_pos += slot_size;
      
      // 处理循环队列：如果读指针到达数据区末尾，回绕到开始
      if (read_pos >= data_max_size) {
        read_pos = 0;  // 回绕到开始位置
      }
      
      head->read_pos_ = read_pos;
      
      // 更新当前消息大小（仅 KEEP_ALL 模式，因为这是队列操作）
      head->cur_msg_size_ = msg_size;
      
      // 通知等待的写者（有空间可以写入新消息）
      pthread_cond_broadcast(&head->cond_);
    }
    // 注意：KEEP_LAST 模式是只读操作，不更新读指针、cur_msg_size_ 和 time_
    // 因为多个读者可以同时读取最新的消息，不应该修改共享状态
    
    // 解锁
    ret = pthread_mutex_unlock(&head->mutex_);
    if (ret != 0) {
      shm.Close();
      throw std::runtime_error("释放互斥锁失败: " + std::string(strerror(ret)));
    }
    
    // 关闭共享内存
    shm.Close();
    
    // 返回实际读取的数据大小
    return msg_size;
    
  } catch (...) {
    pthread_mutex_unlock(&head->mutex_);
    shm.Close();
    throw;
  }
}