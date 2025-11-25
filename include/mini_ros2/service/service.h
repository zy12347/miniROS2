#pragma once
#include <sys/eventfd.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/communication/shm_manager.h"
#include "mini_ros2/logger.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"

class ServiceBase {
 public:
  virtual ~ServiceBase() = default;
  // 可添加通用接口（如取消订阅）
  // virtual void execute() = 0;

  virtual std::function<void()> createTaskFromService() = 0;
};

class Node;
// 需要先声明消息类别才能创建订阅者,消息类需包含serialize和deserialize方法
//  例如:class JsonValue { public: std::string serialize() const; static Json
template <typename MsgT>
class Service : public ServiceBase {
  friend class Node;

 public:
  Service(const std::string& topic, const std::string& event,std::function<void(MsgT& data)> callback) : topic_(topic), event_(event) {
    setCallback(callback);
  };
  ~Service() = default;

  void setCallback(std::function<void(MsgT& data)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
  }
  void setHostId(int host_id) { host_id_ = host_id; };

  std::string getServiceName() const { return topic_ + "_" + event_; }

  void execute(std::shared_ptr<MsgT> msg_ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t task_init_time_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    callback_(*msg_ptr);  // callback 可以修改 msg_ptr 来设置响应
    uint64_t task_end_time_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    uint64_t task_duration = task_end_time_stamp - task_init_time_stamp;
    // LOGD("service " << getServiceName() << " execute duration: " << task_duration << " us");
    shm_->Write(msg_ptr->serialize().c_str(), msg_ptr->serialize().size());
    SHM_MANAGER->triggerEventResponse(topic_, event_);
    LOGD("service " << getServiceName() << " execute response success");
  }

  std::function<void()> createTaskFromService() {
    std::lock_guard<std::mutex> lock(mutex_);
    getMessage();
    std::shared_ptr<MsgT> msg_ptr = std::make_shared<MsgT>(msg_);
    return [this, msg_ptr]() { this->execute(msg_ptr); };
  }

 private:
  void getMessage() {
    try {
      if (shm_ == nullptr) {
        // 订阅时创建共享内存
        std::string shm_name = topic_ + "_" + event_;
        shm_ = std::make_shared<ShmBase>(shm_name);
        shm_->Open();
        LOGD("create shm_name: " << shm_name << " event: " << event_ << " for subscriber");
        // link("/proc/self/fd/" + std::to_string(efd), eventfd_path_.c_str());
      }
      size_t msg_serialize_size = shm_->getDataSize();
      LOGD("msg_serialize_size: " << msg_serialize_size);
      uint8_t* data = new uint8_t[msg_serialize_size];
      shm_->ReadUnlocked(data, msg_serialize_size);
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
  std::function<void(MsgT& data)> callback_;  // 非 const 引用，允许修改响应
  int depth_;
  int host_id_;
  long long time_stamp_ = 0;
  MsgT msg_;
  // int event_fd_ = -1;
  // std::string eventfd_path_;
  // EventSource event_src_;
};