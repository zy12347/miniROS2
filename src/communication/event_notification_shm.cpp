#include "mini_ros2/communication/event_notification_shm.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <sys/mman.h>

EventNotificationShm::EventNotificationShm() {
  shm_ = std::make_shared<SharedMemory>(EVENT_NOTIFICATION_SHM_NAME,
                                        EVENT_NOTIFICATION_SHM_SIZE);
}

EventNotificationShm::~EventNotificationShm() {
  // 清理共享内存
  if (shm_) {
    if (is_owner_ && shm_->IsOwner()) {
      std::cout << "EventNotificationShm destructor: cleaning up shared memory " 
                << EVENT_NOTIFICATION_SHM_NAME << std::endl;
      // SharedMemory 的析构函数会自动调用 Unlink() 如果 is_owner_ 为 true
      // 但我们需要先关闭，然后让 SharedMemory 的析构函数处理 Unlink
      shm_->Close();
      // 注意：不要在这里调用 Unlink()，让 SharedMemory 的析构函数处理
    } else {
      // 不是创建者，只关闭
      shm_->Close();
    }
    // SharedMemory 的析构函数会自动处理 Unlink（如果是 owner）
    shm_.reset();
  }
}

bool EventNotificationShm::Exists() const { return shm_->Exists(); }

void EventNotificationShm::Create() {
  if (!shm_->Create()) {
    throw std::runtime_error("Failed to create event notification shared memory");
  }
  is_owner_ = true;
  Open();
  initMutexAndCond();
}

void EventNotificationShm::Open() {
  if (!shm_->Open()) {
    throw std::runtime_error("Failed to open event notification shared memory");
  }

  EventNotificationData* head = static_cast<EventNotificationData*>(shm_->Data());
  if (!head) {
    throw std::runtime_error("Failed to get event notification shared memory pointer");
  }

  // 检查是否已初始化
  if (is_owner_ || head->initialized_ != 0x4556454E) {  // "EVEN"
    initMutexAndCond();
  } else {
    cachePointers();
  }
}

void EventNotificationShm::initMutexAndCond() {
  EventNotificationData* head = static_cast<EventNotificationData*>(shm_->Data());
  if (!head) {
    throw std::runtime_error("Failed to get event notification shared memory pointer");
  }

  // 初始化互斥锁
  pthread_mutexattr_t mutex_attr;
  int ret = pthread_mutexattr_init(&mutex_attr);
  if (ret != 0) {
    throw std::runtime_error("Failed to init mutex attr: " + std::string(strerror(ret)));
  }

  ret = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    throw std::runtime_error("Failed to set mutex shared: " + std::string(strerror(ret)));
  }

  ret = pthread_mutex_init(&head->mutex_, &mutex_attr);
  if (ret != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    throw std::runtime_error("Failed to init mutex: " + std::string(strerror(ret)));
  }
  pthread_mutexattr_destroy(&mutex_attr);

  // 初始化条件变量
  pthread_condattr_t cond_attr;
  ret = pthread_condattr_init(&cond_attr);
  if (ret != 0) {
    throw std::runtime_error("Failed to init cond attr: " + std::string(strerror(ret)));
  }

  ret = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("Failed to set cond shared: " + std::string(strerror(ret)));
  }

  ret = pthread_cond_init(&head->cond_, &cond_attr);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("Failed to init cond: " + std::string(strerror(ret)));
  }
  pthread_condattr_destroy(&cond_attr);

  // 设置初始化标志
  head->initialized_ = 0x4556454E;  // "EVEN"
  head->event_flag_ = 0;
  head->time_ = 0;

  cachePointers();
}

void EventNotificationShm::cachePointers() {
  EventNotificationData* head = static_cast<EventNotificationData*>(shm_->Data());
  if (!head) {
    throw std::runtime_error("Failed to get event notification shared memory pointer");
  }

  data_ptr_ = head;
  mutex_ptr_ = &head->mutex_;
  cond_ptr_ = &head->cond_;
  event_flag_ptr_ = &head->event_flag_;
}

void EventNotificationShm::triggerEvent(int event_id) {
  if (event_id < 0 || event_id >= 32) {
    return;  // 无效的 event_id
  }

  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }

  try {
    // 设置对应的位
    *event_flag_ptr_ |= (1 << event_id);
    // 更新时间戳
    data_ptr_->time_ = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    // 通知所有等待的线程
    pthread_cond_broadcast(cond_ptr_);
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }
}

uint32_t EventNotificationShm::waitForEvent(uint64_t timeout_ms) {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }

  uint32_t event_flag = 0;
  try {
    // 等待条件变量（带超时）
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += timeout_ms / 1000;
    abstime.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (abstime.tv_nsec >= 1000000000) {
      abstime.tv_sec += 1;
      abstime.tv_nsec -= 1000000000;
    }

    ret = pthread_cond_timedwait(cond_ptr_, mutex_ptr_, &abstime);
    if (ret == ETIMEDOUT) {
      // 超时，返回当前的事件标志位（可能为0）
      event_flag = *event_flag_ptr_;
    } else if (ret != 0) {
      pthread_mutex_unlock(mutex_ptr_);
      throw std::runtime_error("Failed to wait cond: " + std::string(strerror(ret)));
    } else {
      // 被唤醒，读取事件标志位
      event_flag = *event_flag_ptr_;
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }

  return event_flag;
}

uint32_t EventNotificationShm::readAndClearEvents() {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }

  uint32_t event_flag = 0;
  try {
    // 读取并清除事件标志位
    event_flag = *event_flag_ptr_;
    *event_flag_ptr_ = 0;
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }

  return event_flag;
}

uint32_t EventNotificationShm::readEvents() const {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }

  uint32_t event_flag = *event_flag_ptr_;

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }

  return event_flag;
}

void EventNotificationShm::notifyAll() {
  if (mutex_ptr_ == nullptr || cond_ptr_ == nullptr) {
    return;  // 未初始化
  }
  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    return;  // 获取锁失败，忽略错误
  }
  // 通知所有等待的线程
  pthread_cond_broadcast(cond_ptr_);

  // 释放锁
  pthread_mutex_unlock(mutex_ptr_);
}

void EventNotificationShm::clearEvents(int event_id) {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }

  *event_flag_ptr_ &= ~(1 << event_id);

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }
}
void EventNotificationShm::clearEvents() {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }

  *event_flag_ptr_ = 0;

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }
}

void EventNotificationShm::lock() {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
  }
}

void EventNotificationShm::unlock() {
  if (mutex_ptr_ == nullptr) {
    throw std::runtime_error("Event notification shared memory not initialized");
  }
  int ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
  }
}

