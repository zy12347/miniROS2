#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "mini_ros2/logger.h"

namespace mini_ros2 {

/**
 * @brief 线程安全的缓冲区池
 * 用于复用消息序列化/反序列化缓冲区，减少内存分配开销
 */
class BufferPool {
 public:
  /**
   * @brief 获取一个缓冲区（如果池中有可用缓冲区则复用，否则分配新的）
   * @param size 需要的缓冲区大小
   * @return 缓冲区指针（调用者不需要释放，由 BufferPool 管理）
   */
  uint8_t* acquire(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果请求的大小超过当前缓冲区，重新分配更大的缓冲区
    if (size > buffer_size_) {
      buffer_size_ = size * 2;  // 分配 2 倍大小，减少重新分配次数
      buffer_.reset(new uint8_t[buffer_size_]);
      LOGD("BufferPool: 重新分配缓冲区，大小: " << buffer_size_);
    }
    
    // 如果缓冲区为空，分配新的
    if (!buffer_) {
      buffer_size_ = std::max(size, size_t(1024));  // 最小 1KB
      buffer_.reset(new uint8_t[buffer_size_]);
      LOGD("BufferPool: 分配新缓冲区，大小: " << buffer_size_);
    }
    
    return buffer_.get();
  }
  
  /**
   * @brief 释放缓冲区（实际上不释放，只是标记为可用）
   * 注意：当前实现中，缓冲区在对象生命周期内一直保持，无需显式释放
   */
  void release(uint8_t* ptr) {
    // 当前实现中，缓冲区在对象生命周期内一直保持
    // 如果需要支持多缓冲区，可以在这里实现归还逻辑
    (void)ptr;  // 避免未使用参数警告
  }
  
  /**
   * @brief 获取当前缓冲区大小
   */
  size_t getBufferSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_size_;
  }
  
  /**
   * @brief 清空缓冲区（释放内存）
   */
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.reset();
    buffer_size_ = 0;
  }

 private:
  std::unique_ptr<uint8_t[]> buffer_;  // 缓冲区
  size_t buffer_size_ = 0;              // 缓冲区大小
  mutable std::mutex mutex_;            // 保护缓冲区的互斥锁
};

}  // namespace mini_ros2

