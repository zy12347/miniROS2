#pragma once
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

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
  Subscriber(std::string &topic) : topic_(topic){};
  ~Subscriber() {
    is_running_ = false;
    if (listen_thread_.joinable()) {
      listen_thread_.join();
    }
  }
  void subscribe(const std::string &event) {
    subscribe(event, [](const MsgT &data) {
      // std::cout << Serializer::serialize(data) << std::endl;
    });
  }
  void subscribe(const std::string &event,
                 std::function<void(const MsgT &data)> callback) {
    setCallback(callback);
    if (is_running_) {
      return;
    }
    if (shm_ == nullptr) {
      // 订阅时创建共享内存
      std::string shm_name = topic_ + "_" + event;
      shm_ = std::make_shared<ShmBase>(shm_name);
    }
    shm_->Open();
  }

  void setCallback(std::function<void(const MsgT &data)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
  }
  void setHostId(int host_id) { host_id_ = host_id; };

  std::string getTopicName() const { return topic_; }

  void execute() {
    // try {
    size_t msg_serialize_size = shm_->getSize();
    std::cout << "msg_serialize_size: " << msg_serialize_size << std::endl;
    uint8_t *data = new uint8_t[msg_serialize_size];
    shm_->Read(data, msg_serialize_size);
    MsgT msg;
    Serializer::deserialize<MsgT>(data, msg_serialize_size, msg);
    callback_(msg);
    delete[] data;
    // } catch (const std::exception &e) {
    //   std::cerr << "Subscription listen error: " << e.what() << "\n";
    // }
  }

private:
  std::mutex mutex_;
  std::string topic_;
  std::shared_ptr<ShmBase> shm_;
  std::function<void(const MsgT &data)> callback_;
  int depth_;
  int host_id_;
  std::atomic<bool> is_running_ = false;
  std::thread listen_thread_;
  long long time_stamp_ = 0;
};