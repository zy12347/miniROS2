#include "mini_ros2/communication/shm_base.h"

void ShmBase::Create() {
  if (!shm_.Create()) {
    throw std::runtime_error("Failed to create shared memory");
  }
  // if (!sem_.Create()) {
  //   throw std::runtime_error("Failed to create semaphore");
  // }
  initMutexAndCond();
}

void ShmBase::initMutexAndCond() {
  ShmHead *head = static_cast<ShmHead *>(shm_.Data());
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
  ret = pthread_cond_init(&head->cond_, &cond_attr);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("初始化条件变量失败：" +
                             std::string(strerror(ret)));
  }
  pthread_condattr_destroy(&cond_attr);

  head->time_ = 0;

  CachePointers(head);
}

void ShmBase::createMutexAndCondForAll() {
  ShmHead *head = static_cast<ShmHead *>(shm_.Data());
  ShmHead *head_for_all = static_cast<ShmHead *>(head + 1);
  if (!head_for_all) {
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
  ret = pthread_cond_init(&head_for_all->cond_, &cond_attr);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("初始化条件变量失败：" +
                             std::string(strerror(ret)));
  }
  pthread_condattr_destroy(&cond_attr);

  head_for_all->time_ = 0;

  // CachePointers(head);/
}

void ShmBase::CachePointers(ShmHead *head) {
  mutex_ptr_ = &head->mutex_;
  cond_ptr_ = &head->cond_;
  time_ptr_ = &head->time_;

  // 数据区紧跟在ShmHead之后
  ShmData *shm_data = reinterpret_cast<ShmData *>(head + 1);
  data_ptr_ = shm_data->data_;
}

bool ShmBase::Exists() const { return shm_.Exists(); }
void ShmBase::Write(const void *data, size_t size, size_t offset) {
  if (offset + size > shm_.Size()) {
    throw std::out_of_range("Write exceeds shared memory size");
  }
  // if (!sem_.Valid()) {
  //   throw std::runtime_error("Semaphore not initialized");
  // }
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // ShmHead *head = static_cast<ShmHead *>(shm_.Data());
  // ShmData *shm_data =
  //     reinterpret_cast<ShmData *>(head + 1); // data_紧跟在ShmHead之后
  // offset += offset_;
  char *write_ptr = data_ptr_ + offset; // 写入起始地址
  // sem_.Wait(); // 获取信号量
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("获取互斥锁失败：" + std::string(strerror(ret)));
  }
  try {
    std::memcpy(data_ptr_, data, size);
    *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  } catch (...) {
    // 出错时解锁，避免死锁
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }
  // 解锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("释放互斥锁失败：" + std::string(strerror(ret)));
  }
  // sem_.Post(); // 释放信号量
}

void ShmBase::Read(void *buffer, size_t size, size_t offset) {
  if (offset + size > shm_.Size()) {
    throw std::out_of_range("read exceeds shared memory size");
  }
  // if (!sem_.Valid()) {
  //   throw std::runtime_error("Semaphore not initialized");
  // }
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  offset += offset_;
  // sem_.Wait(); // 获取信号量
  // ShmHead *head = static_cast<ShmHead *>(shm_.Data());
  // ShmData *shm_data = reinterpret_cast<ShmData *>(head + 1);
  const char *read_ptr = data_ptr_ + offset; // 读取起始地址

  // 加锁（互斥访问）
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("获取互斥锁失败：" + std::string(strerror(ret)));
  }
  try {
    // 读取数据
    // memcpy(buffer, read_ptr, size);
    std::memcpy(buffer, read_ptr, size);
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }
  // 解锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("释放互斥锁失败：" + std::string(strerror(ret)));
  }
  // sem_.Post(); // 释放信号量
}

void ShmBase::Close() {
  if (!shm_.Close()) {
    throw std::runtime_error("Failed to close shared memory");
  }
}