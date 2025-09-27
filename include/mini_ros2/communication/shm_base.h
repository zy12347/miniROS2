#pragma once
#include "mysemaphore.h"
#include "shared_memory.h"
#include <cstring>

class ShmBase {
public:
  ShmBase(const std::string &name, size_t size)
      : name_(name), size_(size), shm_(name, size),
        sem_(name + "_sem", 1) { // 信号量初始值为1
  }

  void Create();
  void Open() {
    if (!shm_.Open()) {
      throw std::runtime_error("Failed to open shared memory");
    }
    if (!sem_.Open()) {
      throw std::runtime_error("Failed to open semaphore");
    }
  }
  void Write(const void *data, size_t size, size_t offset = 0);

  void Read(void *buffer, size_t size, size_t offset = 0);

  ~ShmBase() {
    shm_.Close();
    // 注意：析构函数不调用unlink，避免提前删除被其他进程使用的共享内存
  }

private:
  SharedMemory shm_;
  MySemaphore sem_;
  std::string name_;
  void *data_;
  size_t size_;
  size_t offset_;
};