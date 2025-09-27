#pragma once

#include <fcntl.h> // For O_* constants
#include <semaphore.h>
#include <stdexcept>
#include <string>
// #pragma message("Custom semaphore.h included, and <semaphore.h> is loaded")

class MySemaphore {
public:
  MySemaphore(const std::string &name, unsigned int value)
      : name_(name), sem_(nullptr), is_owner_(false), value_(value) {
    if (name_.empty() || name_[0] != '/' ||
        name_.find('/', 1) != std::string::npos) {
      throw std::invalid_argument("Invalid name for Semaphore");
    }
  }

  MySemaphore(const MySemaphore &) = delete;            // 禁止拷贝构造
  MySemaphore &operator=(const MySemaphore &) = delete; // 禁止拷贝赋值

  MySemaphore(
      MySemaphore &&other) noexcept; //右值引用 移动构造函数必须是右值引用
                                     //与拷贝构造区分 允许函数接管参数的所有资源
  MySemaphore &operator=(MySemaphore &&other) noexcept;

  bool Create();                            //创建信号量
  bool Open();                              //打开已存在的信号量
  void Wait();                              // P操作,等待信号量
  void Post();                              // V操作,释放信号量
  bool Valid() { return sem_ != nullptr; }; //返回信号量句柄

  ~MySemaphore();

  static bool Unlink(const std::string &name); //删除信号量

private:
  std::string name_; //信号量名称(进程间通信需唯一标识)
  unsigned int value_ = 1;
  sem_t *
      sem_; //信号量句柄 跨进程可见 不可拷贝
            //用于协调多个进程或线程对共享资源访问的同步机制，本质上是一个受保护的整数计数器，通过定义两种原子操作（P操作和V
            //操作）来控制资源的分配与释放
  bool is_owner_; //是否为创建者
};