#include "mini_ros2/communication/shared_memory.h"

SharedMemory::~SharedMemory() {
  if (data_ != MAP_FAILED && data_ != nullptr) {
    Close();
  }
  if (is_owner_) {
    Unlink();
  }
}

bool SharedMemory::Create() {
  if (is_owner_) {
    return true; // 已经是创建者，直接返回
  }
  std::cout << name_ << std::endl;
  fd_ =
      shm_open(name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
               0666); //创建共享内存并可读可写,如果已存在则创建失败.0666表示权限
  if (fd_ == -1) {
    std::cout << "shm_open failed: " << name_ << std::endl;
    return false; // 创建失败
  }
  if (ftruncate(fd_, size_) == -1) { //设置共享内存的大小
    Close();
    shm_unlink(name_.c_str());
    std::cout << "ftruncate failed" << std::endl;
    return false; // 设置大小失败
  }
  data_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
               0); //指针,大小,权限,映射模式,文件描述符,起始位置开始映射
  if (data_ == MAP_FAILED) {
    Close();
    shm_unlink(name_.c_str());
    std::cout << "mmap failed" << std::endl;
    return false; // 映射失败
  }
  is_owner_ = true;
  return true;
}

bool SharedMemory::Exists() const {
  int fd = shm_open(name_.c_str(), O_RDONLY, 0666);
  if (fd == -1) {
    return false; // 不存在
  }
  close(fd);
  return true;
}

bool SharedMemory::Open() {
  if (is_owner_) {
    return true; // 已经是创建者，直接返回
  }
  std::cout << name_ << std::endl;
  fd_ = shm_open(name_.c_str(), O_RDWR, 0666);
  if (fd_ == -1) {
    std::cout << "shm_open failed" << std::endl;
    return false; // 打开失败
  }
  data_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (data_ == MAP_FAILED) {
    Close();
    std::cout << "mmap failed" << std::endl;
    return false; // 映射失败
  }
  return true;
}

bool SharedMemory::Close() {
  if (data_ && data_ != MAP_FAILED) {
    munmap(data_, size_);
    data_ = nullptr;
  }
  if (fd_ != -1) {
    ::close(fd_);
    fd_ = -1;
  }
  is_owner_ = false;
  return true;
}

bool SharedMemory::Unlink() {
  if (!is_owner_) {
    return false; // 不是创建者，不能删除
  }
  if (shm_unlink(name_.c_str()) == -1) {
    return false; // 删除失败
  }
  return true;
}

void *SharedMemory::Data() { return data_; }