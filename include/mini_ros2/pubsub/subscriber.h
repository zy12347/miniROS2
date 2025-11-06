#pragma once
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <sys/eventfd.h>
#include <unistd.h>

class SubscriberBase {
public:
  virtual ~SubscriberBase() = default;
  // 可添加通用接口（如取消订阅）
  virtual void execute() = 0;
};

class Node;
//需要先声明消息类别才能创建订阅者,消息类需包含serialize和deserialize方法
// 例如:class JsonValue { public: std::string serialize() const; static Json
template <typename MsgT> class Subscriber : public SubscriberBase {
  friend class Node;

public:
  // Subscriber(const Subscriber &) = delete;
  // Subscriber &operator=(const Subscriber &) = delete;
  // Subscriber(Subscriber &&) = delete;
  // Subscriber &operator=(Subscriber &&) = delete;
  Subscriber(const std::string &topic) : topic_(topic){};
  ~Subscriber(){
      // if (event_fd_ != -1) {
      //   close(event_fd_);
      //   if (!eventfd_path_.empty()) {
      //     unlink(eventfd_path_.c_str()); // 删除文件系统链接
      //   }
      // }
  };
  void subscribe(const std::string &event) {
    subscribe(event, [](const MsgT &data) {
      // std::cout << Serializer::serialize(data) << std::endl;
    });
  }
  void subscribe(const std::string &event,
                 std::function<void(const MsgT &data)> callback) {
    setCallback(callback);
    if (shm_ == nullptr) {
      // 订阅时创建共享内存
      std::string shm_name = topic_ + "_" + event;
      shm_ = std::make_shared<ShmBase>(shm_name);
      // link("/proc/self/fd/" + std::to_string(efd), eventfd_path_.c_str());
    }
    // if (eventfd_path_.empty() || event_fd_ == -1) {
    //   initEventFd();
    // }
    shm_->Open();
  }

  void setCallback(std::function<void(const MsgT &data)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
  }
  void setHostId(int host_id) { host_id_ = host_id; };

  std::string getTopicName() const { return topic_; }

  // std::string getEventFdPath() {
  //   if (eventfd_path_.empty()) {
  //     throw std::runtime_error("Subscriber: do not have eventfd");
  //   }
  //   return eventfd_path_;
  // }

  // void initEventFd() {
  //   std::string shm_name = "/tmp" + shm_->getShmName() + "_eventfd";
  //   eventfd_path_ = shm_name; // + "_" + std::to_string(getpid());
  //   event_fd_ = eventfd(0, EFD_NONBLOCK);
  //   if (event_fd_ == -1) {
  //     throw std::runtime_error("Sublisher: failed to create ack eventfd " +
  //                              std::string(strerror(errno)));
  //   }
  //   std::string efd_path = "/proc/self/fd/" + std::to_string(event_fd_);
  //   struct stat st;
  //   if (stat(efd_path.c_str(), &st) == -1) {
  //     close(event_fd_);
  //     throw std::runtime_error("Subscriber: src path invalid: " + efd_path +
  //                              " " + std::string(strerror(errno)));
  //   }
  //   std::cout << "efd_path " << efd_path << " eventfd_path_ " <<
  //   eventfd_path_
  //             << std::endl;
  //   if (link(efd_path.c_str(), eventfd_path_.c_str()) == -1) {
  //     close(event_fd_);
  //     throw std::runtime_error("Subscriber: failed to link ack eventfd " +
  //                              std::string(strerror(errno)));
  //   }
  //   event_src_.efd_name = eventfd_path_;
  //   event_src_.efd = event_fd_;
  //   event_src_.type = EVENT_TYPE_SUB;
  //   event_src_.data = this; // 传递当前 Subscriber 实例作为业务数据
  //   // 回调函数：读取共享内存消息并触发用户回调
  //   event_src_.callback = [this]() {
  //     // 触发用户注册的回调
  //     this->execute();
  //   };
  // }

  // void getEventSrc(EventSource &ev) {
  //   ev = event_src_;
  //   return;
  // }

  void execute() {
    try {
      size_t msg_serialize_size = shm_->getDataSize();
      std::cout << "msg_serialize_size: " << msg_serialize_size << std::endl;
      uint8_t *data = new uint8_t[msg_serialize_size];
      shm_->Read(data, msg_serialize_size);
      MsgT msg;
      Serializer::deserialize<MsgT>(data, msg_serialize_size, msg);
      callback_(msg);
      delete[] data;
    } catch (const std::exception &e) {
      std::cerr << "Subscription listen error: " << e.what() << "\n";
    }
  }

private:
  std::mutex mutex_;
  std::string topic_;
  std::shared_ptr<ShmBase> shm_;
  std::function<void(const MsgT &data)> callback_;
  int depth_;
  int host_id_;
  long long time_stamp_ = 0;
  // int event_fd_ = -1;
  // std::string eventfd_path_;
  // EventSource event_src_;
};