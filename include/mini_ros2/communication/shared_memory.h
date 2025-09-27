#include <stdexcept>
#include <string>
class SharedMemory {
public:
  SharedMemory(std::string name, size_t size)
      : name_(name), size_(size), fd_(-1), data_(nullptr), is_owner_(false) {
    // POSIX标准要求共享内存名称以'/'开头且不包含其他'/'且共享内存要小于10MB
    if (name_.empty() || name_[0] == '/' || size_ == 0 ||
        size_ > 10 * 1024 * 1024) {
      throw std::invalid_argument("Invalid name or size for SharedMemory");
    }
    //初始化元信息不执行系统调用,延迟资源获取
  };
  ~SharedMemory();

  bool create(); //创建共享内存
  bool open();   //打开已存在的共享内存
  void *data(); //返回指向共享内存的指针,无类型指针,使用时必须强制转换
  bool close();  //关闭共享内存
  bool unlink(); //删除共享内存
private:
  std::string name_; //共享内存名称
  size_t size_;      //共享内存大小
  int fd_;           //文件描述符
  void *data_;       //指向共享内存的指针
  bool is_owner_;    //是否是创建者
};