#pragma once
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
class SharedMemory {
public:
  SharedMemory() = default;
  SharedMemory(std::string name, size_t size)
      : name_(name), size_(size), fd_(-1), data_(nullptr), is_owner_(false) {
    // POSIX标准要求共享内存名称以'/'开头且不包含其他'/'且共享内存要小于10MB
    if (name_.empty() || name_[0] != '/' || size_ == 0 ||
        size_ > 10 * 1024 * 1024) {
      std::cout << name_ << " " << size_ << std::endl;
      throw std::invalid_argument("Invalid name or size for SharedMemory");
    }
    //初始化元信息不执行系统调用,延迟资源获取
  };
  SharedMemory(std::string name) //仅有名字没有大小,常用于订阅已存在的共享内存
      : name_(name), fd_(-1), data_(nullptr), is_owner_(false) {
    // POSIX标准要求共享内存名称以'/'开头且不包含其他'/'且共享内存要小于10MB
    if (name_.empty() || name_[0] != '/' || !Exists()) {
      std::cout << name_ << std::endl;
      throw std::invalid_argument("Not exist");
    }
    // 1. 打开已存在的POSIX共享内存
    int shm_fd = shm_open(name_.c_str(), O_RDONLY, 0666); // O_RDONLY：只读打开
    if (shm_fd == -1) {
      throw std::invalid_argument("Can not Open " + name_);
    }

    // 2. 获取共享内存文件状态（包含大小）
    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1) {
      throw std::invalid_argument("Can not get Stat");
      ::close(shm_fd);
    }
    size_ = shm_stat.st_size; // 共享内存大小
    //初始化元信息不执行系统调用,延迟资源获取
  };
  ~SharedMemory();

  bool Create();       //创建共享内存
  bool Open();         //打开已存在的共享内存
  bool Exists() const; //检查共享内存是否已存在
  void *Data(); //返回指向共享内存的指针,无类型指针,使用时必须强制转换
  bool Close();  //关闭共享内存
  bool Unlink(); //删除共享内存
  size_t Size() const { return size_; }
  bool IsOwner() const { return is_owner_; } //检查是否是共享内存的创建者

  // 引用计数相关
  bool IncrementRefCount(); // 增加引用计数
  bool DecrementRefCount(); // 减少引用计数
  int GetRefCount();        // 获取当前引用计数

private:
  std::string name_; //共享内存名称
  size_t size_;      //共享内存大小
  int fd_;           //文件描述符
  void *data_;       //指向共享内存的指针
  bool is_owner_;    //是否是创建者
};