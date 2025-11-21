#pragma once
#include <pthread.h>

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

#include "mysemaphore.h"
#include "shared_memory.h"

struct ShmHead {
  uint32_t initialized_;  // 初始化标志：0x4D525332 = "MRS2" (MiniROS2)
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  uint64_t time_;
};

struct ShmData {
  char data_[0];  // 荣幸数组
};

class ShmBase {
 public:
  ShmBase() = default;
  ShmBase(const std::string& name, size_t size)
      : name_(name),
        offset_(sizeof(ShmHead)),
        data_size_(size),
        total_size_(offset_ + data_size_),
        shm_(name, total_size_) {  // 信号量初始值为1
  }
  ShmBase(const std::string& name) : name_(name), shm_(name) {
    total_size_ = shm_.Size();
    offset_ = sizeof(ShmHead);
    data_size_ = total_size_ - offset_;
  }

  void Create();
  bool Exists() const;  // 检查共享内存是否存在
  void Open() {
    if (!shm_.Open()) {
      throw std::runtime_error("Failed to open shared memory");
    }
    ShmHead* head = static_cast<ShmHead*>(shm_.Data());
    if (!head) {
      throw std::runtime_error("获取共享内存头部指针失败");
    }
    // 只有在创建新的共享内存时才初始化互斥锁和条件变量
    // 如果打开已存在的共享内存，只缓存指针，不重新初始化

    // 使用双重检查：IsOwner() OR 未初始化标志
    // 原因：如果创建进程崩溃，互斥锁可能未初始化，需要重新初始化
    // 检查初始化标志位（magic number "MRS2" = 0x4D525332）
    if (shm_.IsOwner() || head->initialized_ != 0x4D525332) {
      initMutexAndCond();
    } else {
      CachePointers(head);
    }
    // if (!sem_.Open()) {
    //   throw std::runtime_error("Failed to open semaphore");
    // }
  }
  void Write(const void* data, size_t size, size_t offset = 0);

  // 内部写入方法：假设调用者已经持有锁（用于避免双重锁）
  void WriteUnlocked(const void* data, size_t size, size_t offset = 0);

  void Read(void* buffer, size_t size, size_t offset = 0);

  void ReadUnlocked(void* buffer, size_t size, size_t offset = 0);

  void Close();

  size_t getSize() const { return total_size_; }

  size_t getDataSize() const { return data_size_; }

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

 private:
  void CachePointers(ShmHead* head);
  // MySemaphore sem_;
  std::string name_;
  size_t offset_;
  size_t data_size_;
  size_t total_size_;
  // ShmHead shm_head_;
  SharedMemory shm_;  // 初始化顺序与声明顺序要保存一致
  pthread_mutex_t* mutex_ptr_ = nullptr;
  pthread_cond_t* cond_ptr_ = nullptr;
  uint64_t* time_ptr_ = nullptr;
  char* data_ptr_;
};