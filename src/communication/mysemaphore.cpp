#include "mysemaphore.h"

MySemaphore::~MySemaphore() {
  if (sem_ != SEM_FAILED && sem_ != nullptr) {
    sem_close(sem_); // 关闭信号量句柄（不删除名称）
    sem_ = nullptr;
    // 注意：析构函数不调用unlink，避免提前删除被其他进程使用的信号量
  }
  if (is_owner_) {
    Unlink(name_); //如果是创建者,删除信号量
  }
}

bool MySemaphore::Create() {
  if (is_owner_) {
    return true; // 已经是创建者，直接返回
  }
  sem_ = sem_open(name_.c_str(), O_CREAT | O_EXCL, 0666,
                  value_); //创建新的信号量,value_表示允许的进程个数
  if (sem_ == SEM_FAILED) {
    perror("sem_open failed");
    return false; // 创建失败
  }
  is_owner_ = true;
  return true;
}

bool MySemaphore::Open() {
  if (is_owner_) {
    return true; // 已经是创建者，直接返回
  }
  sem_ = sem_open(name_.c_str(), 0); //打开已有信号量句柄
  if (sem_ == SEM_FAILED) {
    perror("sem_open failed");
    return false; // 打开失败
  }
  return true;
}

void MySemaphore::Wait() {
  if (sem_ == SEM_FAILED || sem_ == nullptr) {
    throw std::runtime_error("Semaphore not initialized");
  }

  if (sem_wait(sem_) == -1) {
    perror("sem_wait failed");
    throw std::runtime_error("sem_wait error");
  }
}

void MySemaphore::Post() {
  if (sem_ == SEM_FAILED || sem_ == nullptr) {
    throw std::runtime_error("Semaphore not initialized");
  }
  // sem_post将计数器+1，若有线程阻塞则唤醒一个
  if (sem_post(sem_) == -1) {
    perror("sem_post failed");
    throw std::runtime_error("sem_post error");
  }
}

bool MySemaphore::Unlink(const std::string &name) {
  if (name.empty() || name[0] != '/' ||
      name.find('/', 1) != std::string::npos) {
    throw std::invalid_argument("Invalid name for Semaphore");
  }
  // 删除命名信号量
  if (sem_unlink(name.c_str()) == -1) {
    perror("sem_unlink failed");
    return false;
  }
  return true;
}

MySemaphore::MySemaphore(MySemaphore &&other) noexcept
    : name_(std::move(other.name_)), sem_(other.sem_),
      is_owner_(other.is_owner_) {
  other.sem_ = nullptr;
  other.is_owner_ = false;
}

// 移动赋值：转移所有权并释放当前资源
MySemaphore &MySemaphore::operator=(MySemaphore &&other) noexcept {
  if (this != &other) {
    // 释放当前资源
    if (sem_ != SEM_FAILED && sem_ != nullptr) {
      sem_close(sem_);
    }
    // 转移other的资源
    name_ = std::move(other.name_);
    sem_ = other.sem_;
    is_owner_ = other.is_owner_;
    // 清空other
    other.sem_ = nullptr;
    other.is_owner_ = false;
  }
  return *this;
}