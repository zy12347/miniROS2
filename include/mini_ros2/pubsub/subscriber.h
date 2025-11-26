#pragma once
#include <sys/eventfd.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/logger.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"
#include "mini_ros2/qos_policy.h"

class SubscriberBase {
 public:
  virtual ~SubscriberBase() = default;
  // 可添加通用接口（如取消订阅）
  // virtual void execute() = 0;

  virtual std::function<void()> createTaskFromSubEvent() = 0;
};

class Node;
// 需要先声明消息类别才能创建订阅者,消息类需包含serialize和deserialize方法
//  例如:class JsonValue { public: std::string serialize() const; static Json
template <typename MsgT>
class Subscriber : public SubscriberBase {
  friend class Node;

 public:
  // Subscriber(const Subscriber &) = delete;
  // Subscriber &operator=(const Subscriber &) = delete;
  // Subscriber(Subscriber &&) = delete;
  // Subscriber &operator=(Subscriber &&) = delete;
  Subscriber(const std::string& topic, QosPolicy qos_policy = QosPolicy()) : topic_(topic), qos_policy_(qos_policy) {};
  ~Subscriber() {
    // if (event_fd_ != -1) {
    //   close(event_fd_);
    //   if (!eventfd_path_.empty()) {
    //     unlink(eventfd_path_.c_str()); // 删除文件系统链接
    //   }
    // }
  };
  void subscribe(const std::string& event) {
    subscribe(event, [](const MsgT& data) {
      // std::cout << Serializer::serialize(data) << std::endl;
    });
  }
  void subscribe(const std::string& event,
                 std::function<void(const MsgT& data)> callback) {
    setCallback(callback);
    event_ = event;
  }

  void setCallback(std::function<void(const MsgT& data)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
  }
  void setHostId(int host_id) { host_id_ = host_id; };

  std::string getTopicName() const { return topic_; }
  void execute(std::shared_ptr<MsgT> msg_ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_(*msg_ptr);
    LOGD("test");
  }

  std::function<void()> createTaskFromSubEvent() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOGD("createTaskFromSubEvent: " << topic_ << " " << event_);
    getMessage();
    LOGD("getMessage: " << msg_.serialize());
    std::shared_ptr<MsgT> msg_ptr = std::make_shared<MsgT>(msg_);
    LOGD("msg_ptr: " << msg_ptr->serialize());
    return [this, msg_ptr]() { this->execute(msg_ptr); };
  }

 private:
  void getMessage() {
    try {
      if (shm_ == nullptr) {
        // 订阅时创建共享内存
        std::string shm_name = topic_ + "_" + event_;
        shm_ = std::make_shared<ShmBase>(shm_name,qos_policy_);
        shm_->Open();
        LOGD("create shm_name: " << shm_name << " event: " << event_ << " for subscriber");
        // link("/proc/self/fd/" + std::to_string(efd), eventfd_path_.c_str());
      }
      // 使用 Read() 方法（带锁），而不是 ReadUnlocked()
      // 因为 Read() 会正确处理锁和读指针更新
      size_t msg_serialize_size = shm_->getCurMsgSize();
      LOGD("msg_serialize_size: " << msg_serialize_size);
      uint8_t* data = new uint8_t[msg_serialize_size];
      shm_->Read(data, msg_serialize_size);  // 使用 Read() 而不是 ReadUnlocked()
      Serializer::deserialize<MsgT>(data, msg_serialize_size, msg_);
      delete[] data;
    } catch (const std::exception& e) {
      LOGE("Subscription listen error: " << e.what());
    }
  }
  std::mutex mutex_;
  std::string topic_;
  std::string event_;
  std::shared_ptr<ShmBase> shm_;
  std::function<void(const MsgT& data)> callback_;
  QosPolicy qos_policy_;
  int host_id_;
  long long time_stamp_ = 0;
  MsgT msg_;
  // int event_fd_ = -1;
  // std::string eventfd_path_;
  // EventSource event_src_;
};