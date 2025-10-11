#include "mini_ros2/communication/shm_base.h"

void ShmBase::Create() {
  if (!shm_.Create()) {
    throw std::runtime_error("Failed to create shared memory");
  }
  if (!sem_.Create()) {
    throw std::runtime_error("Failed to create semaphore");
  }
}

bool ShmBase::Exists() const { return shm_.Exists(); }
void ShmBase::Write(const void *data, size_t size, size_t offset) {
  if (offset + size > shm_.Size()) {
    throw std::out_of_range("Write exceeds shared memory size");
  }
  if (!sem_.Valid()) {
    throw std::runtime_error("Semaphore not initialized");
  }
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }
  sem_.Wait(); // 获取信号量
  std::memcpy(static_cast<uint8_t *>(shm_.Data()) + offset, data, size);
  sem_.Post(); // 释放信号量
}

void ShmBase::Read(void *buffer, size_t size, size_t offset) {
  if (offset + size > shm_.Size()) {
    throw std::out_of_range("read exceeds shared memory size");
  }
  if (!sem_.Valid()) {
    throw std::runtime_error("Semaphore not initialized");
  }
  if (shm_.Data() == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }
  sem_.Wait(); // 获取信号量
  std::memcpy(buffer, static_cast<uint8_t *>(shm_.Data()) + offset, size);
  sem_.Post(); // 释放信号量
}

void ShmBase::Close() {
  if (!shm_.Close()) {
    throw std::runtime_error("Failed to close shared memory");
  }
}