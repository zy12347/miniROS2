#pragma once
#include "mysemaphore.h"
#include "shared_memory.h"
#include <chrono>
#include <cstring>
#include <pthread.h>
#include <stdexcept>

struct ShmHead {
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  uint64_t time_;
};

struct ShmData {
  char data_[0]; //荣幸数组
};

class ShmBase {
public:
  ShmBase() = default;
  ShmBase(const std::string &name, size_t size)
      : name_(name), offset_(sizeof(ShmHead)), data_size_(size),
        total_size_(offset_ + data_size_),
        shm_(name, total_size_) { // 信号量初始值为1
  }
  ShmBase(const std::string &name) : name_(name), shm_(name) {
    total_size_ = shm_.Size();
    offset_ = sizeof(ShmHead);
    data_size_ = total_size_ - offset_;
  }

  void Create();
  bool Exists() const; // 检查共享内存是否存在
  void Open() {
    if (!shm_.Open()) {
      throw std::runtime_error("Failed to open shared memory");
    }
    // if (!sem_.Open()) {
    //   throw std::runtime_error("Failed to open semaphore");
    // }
  }
  void Write(const void *data, size_t size, size_t offset = 0);

  void Read(void *buffer, size_t size, size_t offset = 0);

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

  void createMutexAndCondForAll();

private:
  void CachePointers(ShmHead *head);
  // MySemaphore sem_;
  std::string name_;
  size_t offset_;
  size_t data_size_;
  size_t total_size_;
  // ShmHead shm_head_;
  SharedMemory shm_; //初始化顺序与声明顺序要保存一致
  pthread_mutex_t *mutex_ptr_ = nullptr;
  pthread_cond_t *cond_ptr_ = nullptr;
  uint64_t *time_ptr_ = nullptr;
  char *data_ptr_;
};