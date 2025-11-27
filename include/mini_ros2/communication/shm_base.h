/**
 * @file shm_base.h
 * @brief 共享内存基类定义
 * @author miniROS2 Team
 * @date 2024
 * 
 * ShmBase 提供了基于共享内存的进程间通信功能，支持 QoS 策略。
 */

#pragma once
#include <pthread.h>

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

#include "mysemaphore.h"
#include "shared_memory.h"
#include "mini_ros2/qos_policy.h"

/**
 * @struct ShmHead
 * @brief 共享内存头部结构
 * 
 * 存储在共享内存的开头，包含元数据和同步原语。
 */
struct ShmHead {
  uint32_t initialized_;  // 初始化标志：0x4D525332 = "MRS2" (MiniROS2)
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  int32_t ref_count_;
  size_t data_size_;
  size_t cur_msg_size_;
  size_t write_pos_;
  size_t read_pos_;
  size_t max_msg_size_;   // 消息最大大小
  size_t slot_size_;      // 每个消息槽的固定大小：sizeof(size_t) + max_msg_size_
  uint64_t time_;
};

/**
 * @class ShmBase
 * @brief 共享内存基类，提供进程间通信的基础设施
 * 
 * ShmBase 封装了共享内存的创建、打开、读写操作，支持：
 * - QoS 策略（KEEP_LAST / KEEP_ALL）
 * - 线程安全的读写操作
 * - 固定大小的消息槽管理
 * - 循环缓冲区支持
 * 
 * @note 通常不直接使用，而是通过 Publisher/Subscriber 使用
 * 
 * @example examples/shared_memory_demo/shm_publisher.cpp
 */
class ShmBase {
 public:
  /**
   * @brief 默认构造函数
   */
  ShmBase() = default;
  
  /**
   * @brief 构造函数（创建新共享内存）
   * @param name 共享内存名称
   * @param size 单个消息的最大大小（字节）
   * @param qos_policy QoS 策略，控制消息的可靠性和历史深度
   */
  ShmBase(const std::string& name, size_t size, QosPolicy qos_policy = QosPolicy())
      : name_(name),
        offset_(sizeof(ShmHead)),
        qos_policy_(qos_policy),
        max_msg_size_(size),  // 消息最大大小
        slot_size_(sizeof(size_t) + size),  // 每个消息槽的固定大小：size_t + data（使用参数 size）
        data_max_size_(slot_size_ * qos_policy_.history_depth),  // 数据区总大小 = 槽大小 * 深度
        total_size_(offset_ + data_max_size_),
        shm_(name, total_size_) {
          LOGD("ShmBase: " << name << " max_msg_size: " << max_msg_size_ 
               << " slot_size: " << slot_size_ 
               << " data_max_size: " << data_max_size_ 
               << " history_depth: " << qos_policy_.history_depth);
  }
  /**
   * @brief 构造函数（打开已存在的共享内存）
   * @param name 共享内存名称
   * @param qos_policy QoS 策略
   * 
   * @note 此构造函数用于打开已存在的共享内存，大小和参数从共享内存头部读取
   */
  ShmBase(const std::string& name,QosPolicy qos_policy = QosPolicy()) : name_(name), shm_(name) ,qos_policy_(qos_policy){
    total_size_ = shm_.Size();
    offset_ = sizeof(ShmHead);
    data_max_size_ = total_size_ - offset_;
    // max_msg_size_ 和 slot_size_ 存储在 ShmHead 中，在 Open() 时读取
    max_msg_size_ = 0;
    slot_size_ = 0;
  }
  ~ShmBase();

  /**
   * @brief 创建共享内存
   * @throw std::runtime_error 如果创建失败
   */
  void Create();
  
  /**
   * @brief 检查共享内存是否存在
   * @return true 如果存在，false 否则
   */
  bool Exists() const;
  
  /**
   * @brief 打开共享内存
   * 
   * 如果共享内存已存在，则打开它；如果是新创建的，则初始化互斥锁和条件变量。
   * 从共享内存头部读取 max_msg_size_ 和 slot_size_。
   * 
   * @throw std::runtime_error 如果打开失败
   */
  void Open() {
    if (!shm_.Open()) {
      throw std::runtime_error("Failed to open shared memory");
    }
    
    // Open() 后重新获取实际大小（因为 Open() 会通过 fstat 更新 size_）
    // 这对于通过名称打开的共享内存很重要
    if (total_size_ == 0 || data_max_size_ == 0) {
      total_size_ = shm_.Size();
      data_max_size_ = (total_size_ > offset_) ? (total_size_ - offset_) : 0;
    }
    
    ShmHead* head = static_cast<ShmHead*>(shm_.Data());
    if (!head) {
      throw std::runtime_error("获取共享内存头部指针失败");
    }
    
    // 从 ShmHead 中读取 slot_size_ 和 max_msg_size_
    if (slot_size_ == 0 || max_msg_size_ == 0) {
      // 从共享内存头部读取
      max_msg_size_ = head->max_msg_size_;
      slot_size_ = head->slot_size_;
      LOGD("从 ShmHead 读取: max_msg_size_ = " << max_msg_size_ 
           << ", slot_size_ = " << slot_size_);
    }
    
    // 只有在创建新的共享内存时才初始化互斥锁和条件变量
    // 如果打开已存在的共享内存，只缓存指针，不重新初始化

    // 使用双重检查：is_creator_ OR 未初始化标志
    // 原因：如果创建进程崩溃，互斥锁可能未初始化，需要重新初始化
    // 检查初始化标志位（magic number "MRS2" = 0x4D525332）
    if (is_creator_ || head->initialized_ != 0x4D525332) {
      initMutexAndCond();
    } else {
      CachePointers(head);
      // 增加引用计数（非创建者）
      incrementRefCount();
    }
    // if (!sem_.Open()) {
    //   throw std::runtime_error("Failed to open semaphore");
    // }
  }

  void clearData(){
      std::memset(data_ptr_, 0, data_max_size_);
  };

  void clearData(void* data_ptr, size_t size){
    std::memset(data_ptr, 0, size);
  };
  
  /**
   * @brief 写入数据到共享内存（线程安全）
   * @param data 要写入的数据指针
   * @param size 数据大小（字节）
   * @param offset 数据区内的偏移量（默认0）
   * @throw std::runtime_error 如果写入失败
   * @throw std::out_of_range 如果数据大小超过限制
   * 
   * @note 根据 QoS 策略自动处理循环缓冲区和队列满的情况
   */
  void Write(const void* data, size_t size, size_t offset = 0);

  /**
   * @brief 写入数据到共享内存（无锁版本）
   * @param data 要写入的数据指针
   * @param size 数据大小（字节）
   * @param offset 数据区内的偏移量（默认0）
   * 
   * @warning 调用者必须确保已持有互斥锁
   * @throw std::runtime_error 如果写入失败
   */
  void WriteUnlocked(const void* data, size_t size, size_t offset = 0);

  /**
   * @brief 从共享内存读取数据（线程安全）
   * @param buffer 接收数据的缓冲区
   * @param size 缓冲区大小（字节）
   * @param offset 数据区内的偏移量（默认0）
   * @throw std::runtime_error 如果读取失败或没有数据可读
   * 
   * @note 对于 KEEP_ALL 模式，读取后会自动更新读指针
   */
  void Read(void* buffer, size_t size, size_t offset = 0);

  /**
   * @brief 从共享内存读取数据（无锁版本）
   * @param buffer 接收数据的缓冲区
   * @param size 缓冲区大小（字节）
   * @param offset 数据区内的偏移量（默认0）
   * 
   * @warning 调用者必须确保已持有互斥锁
   * @throw std::runtime_error 如果读取失败或没有数据可读
   */
  void ReadUnlocked(void* buffer, size_t size, size_t offset = 0);

  void Close();

  // 获取共享内存总大小（包括 ShmHead）
  size_t getSize() const { return total_size_; }
  
  // 获取底层 SharedMemory 的实际大小
  size_t getShmActualSize() const { return shm_.Size(); }

  // 获取数据区最大大小（不包括 ShmHead）
  size_t getDataMaxSize() const { return data_max_size_; }
  
  // 获取消息最大大小
  size_t getMaxMsgSize() const { 
    return max_msg_size_ptr_ ? *max_msg_size_ptr_ : max_msg_size_;
  }
  
  // 获取消息槽大小
  size_t getSlotSize() const { 
    return slot_size_ptr_ ? *slot_size_ptr_ : slot_size_;
  }
  
  // 获取当前可用的数据区大小（考虑 write_pos）
  size_t getAvailableDataSize() const {
    if (write_pos_ptr_ == nullptr) {
      return data_max_size_;
    }
    return data_max_size_ - *write_pos_ptr_;
  }

  size_t getDataSize() const {
    if (data_size_ptr_ == nullptr) {
      return 0;
    }
    pthread_mutex_lock(mutex_ptr_);
    size_t size = *data_size_ptr_;
    pthread_mutex_unlock(mutex_ptr_);
    return size;
  }

  size_t getCurMsgSize() const {
    if (cur_msg_size_ptr_ == nullptr) {
      return 0;
    }
    pthread_mutex_lock(mutex_ptr_);
    size_t size = *cur_msg_size_ptr_;
    pthread_mutex_unlock(mutex_ptr_);
    return size;
  }

  std::string getShmName() {
    if (name_.empty()) {
      throw std::runtime_error("shm do not init");
    }
    return name_;
  }

  void initMutexAndCond();

  void shmBaseLock() {
    int ret = pthread_mutex_lock(mutex_ptr_);
    if (ret != 0) {
      throw std::runtime_error("获取互斥锁失败：" + std::string(strerror(ret)));
    }
  }

  void shmBaseUnlock() {
    int ret = pthread_mutex_unlock(mutex_ptr_);
    if (ret != 0) {
      throw std::runtime_error("释放互斥锁失败： " +
                               std::string(strerror(ret)));
    }
  }

  void shmBaseWait() { pthread_cond_wait(cond_ptr_, mutex_ptr_); }

  void shmBaseWaitTimeOut(uint timeout_ms) {
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += timeout_ms / 1000;
    abstime.tv_nsec += (timeout_ms % 1000) * 1000000;
    // std::cout << "shmBaseWaitTimeOut: " << timeout_ms << std::endl;
    int ret = pthread_cond_timedwait(cond_ptr_, mutex_ptr_, &abstime);
    // if (ret == ETIMEDOUT) {
    //   return;
    // } else if (ret != 0) {
    //   throw std::runtime_error("条件变量等待失败：" +
    //                            std::string(strerror(ret)));
    // }
  }

  void shmBaseSignal() { pthread_cond_signal(cond_ptr_); }

  void shmBaseBroadcast() { pthread_cond_broadcast(cond_ptr_); }

  // 引用计数管理
  void incrementRefCount();
  void decrementRefCount();

  template<typename T>
  static void readDataInfo(std::string shm_name,T& data){
    SharedMemory shm(shm_name);
    shm.Open();
    ShmHead* head = static_cast<ShmHead*>(shm.Data());
    if (!head) {
      throw std::runtime_error("获取共享内存头部指针失败");
    }
    pthread_mutex_lock(&head->mutex_);
    char* data_ptr = reinterpret_cast<char*>(head) + sizeof(ShmHead);
    memcpy(&data, data_ptr, head->cur_msg_size_);
    pthread_mutex_unlock(&head->mutex_);
  };

  // 打印指定名称的共享内存内的所有数据（静态方法）
  static void PrintShmData(const std::string& shm_name, bool hex_dump = false);
  // static void readDataInfo(std::string shm_name,void* data, size_t size){
  //   SharedMemory shm(shm_name);
  //   shm.Open();
  //   ShmHead* head = static_cast<ShmHead*>(shm.Data());
  //   if (!head) {
  //     throw std::runtime_error("获取共享内存头部指针失败");
  //   }
  //   pthread_mutex_lock(&head->mutex_);
  //   memcpy(&data, head->data_ptr_, head->cur_msg_size_);
  //   pthread_mutex_unlock(&head->mutex_);
  // };

 private:
  void CachePointers(ShmHead* head);
  // MySemaphore sem_;
  // 注意：成员变量初始化顺序按照声明顺序，不是初始化列表顺序！
  // 因此 qos_policy_ 必须在 data_max_size_ 之前声明
  QosPolicy qos_policy_;  // 必须在 data_max_size_ 之前声明
  std::string name_;
  size_t offset_;
  size_t max_msg_size_;    // 消息最大大小
  size_t slot_size_;       // 每个消息槽的固定大小：sizeof(size_t) + max_msg_size_
  size_t data_max_size_;   // 数据区总大小：slot_size_ * history_depth
  size_t total_size_;
  // ShmHead shm_head_;
  SharedMemory shm_;  // 初始化顺序与声明顺序要保存一致
  pthread_mutex_t* mutex_ptr_ = nullptr;
  pthread_cond_t* cond_ptr_ = nullptr;
  uint64_t* time_ptr_ = nullptr;
  int32_t* ref_count_ptr_ = nullptr;  // 引用计数指针
  size_t* data_size_ptr_ = nullptr;
  size_t* cur_msg_size_ptr_ = nullptr;
  size_t* max_msg_size_ptr_ = nullptr;  // 消息最大大小指针
  size_t* slot_size_ptr_ = nullptr;      // 消息槽大小指针
  char* data_ptr_;
  size_t* write_pos_ptr_ = nullptr;
  size_t* read_pos_ptr_ = nullptr;
  bool is_creator_ = false;  // 是否是创建者（用于判断是否初始化 ref_count_）
};