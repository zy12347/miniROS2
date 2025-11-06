// miniROS2/include/mini_ros2/communication/shm_cond.h
#pragma once
#include <pthread.h>
#include <stdexcept>
#include <sys/mman.h>

struct ShmCondData {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool data_ready; // 数据就绪标志
};

class ShmCond {
public:
  ShmCond(void *shm_ptr) : cond_data_(static_cast<ShmCondData *>(shm_ptr)) {}

  // 初始化共享条件变量（仅创建者调用）
  void Init() {
    pthread_mutexattr_t mutex_attr;
    pthread_condattr_t cond_attr;

    // 设置互斥锁跨进程共享
    if (pthread_mutexattr_init(&mutex_attr) != 0)
      throw std::runtime_error("mutex attr init failed");
    if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED) != 0)
      throw std::runtime_error("set mutex shared failed");
    if (pthread_mutex_init(&cond_data_->mutex, &mutex_attr) != 0)
      throw std::runtime_error("mutex init failed");

    // 设置条件变量跨进程共享
    if (pthread_condattr_init(&cond_attr) != 0)
      throw std::runtime_error("cond attr init failed");
    if (pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED) != 0)
      throw std::runtime_error("set cond shared failed");
    if (pthread_cond_init(&cond_data_->cond, &cond_attr) != 0)
      throw std::runtime_error("cond init failed");

    cond_data_->data_ready = false;
  }

  // 等待数据就绪
  void Wait() {
    pthread_mutex_lock(&cond_data_->mutex);
    while (!cond_data_->data_ready) { // 防止虚假唤醒
      pthread_cond_wait(&cond_data_->cond, &cond_data_->mutex);
    }
    cond_data_->data_ready = false; // 重置标志
    pthread_mutex_unlock(&cond_data_->mutex);
  }

  // 通知数据就绪
  void Signal() {
    pthread_mutex_lock(&cond_data_->mutex);
    cond_data_->data_ready = true;
    pthread_cond_signal(&cond_data_->cond);
    pthread_mutex_unlock(&cond_data_->mutex);
  }

  // 销毁资源（仅创建者调用）
  void Destroy() {
    pthread_mutex_destroy(&cond_data_->mutex);
    pthread_cond_destroy(&cond_data_->cond);
  }

private:
  ShmCondData *cond_data_;
};