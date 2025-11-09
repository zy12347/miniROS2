#pragma once
#include <sys/eventfd.h>

#include <iostream>
#include <memory>
#include <string>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"

class PublisherBase {
 public:
  virtual ~PublisherBase() = default;  // 虚析构函数
  // 可添加通用接口（如关闭发布者）
};

class Node;
class ShmManager;
#define POST_EVENT(...) Publisher::Publish(__VA_ARGS__)

template <typename MsgT>
class Publisher : public PublisherBase {
  friend class Node;

 public:
  Publisher(std::string& topic) : topic_(topic), shm_manager_(nullptr) {};

  Publisher(std::string& topic, int host_id)
      : topic_(topic), host_id_(host_id), shm_manager_(nullptr) {};
  // Publisher(const Publisher &) = delete;
  // Publisher &operator=(const Publisher &) = delete;
  // Publisher(Publisher &&) = delete;
  // Publisher &operator=(Publisher &&) = delete;

  ~Publisher() = default;
  int publish(const std::string& event, const MsgT& data, int depth = 10) {
    size_t msg_serialize_size;
    msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
    // std::cout << "msg_serialize_size: " << msg_serialize_size << std::endl;
    uint8_t* buffer = new uint8_t[msg_serialize_size];
    Serializer::serialize<MsgT>(data, buffer, msg_serialize_size);
    std::string topic_str = topic_ + "_" + event;
    if (shm_ == nullptr) {
      shm_ = std::make_shared<ShmBase>(topic_str, msg_serialize_size);
      shm_->Create();
      shm_->Open();
    }
    // std::cout << data_str << std::endl;
    // shm_->Open();
    shm_->Write(buffer, msg_serialize_size);
    // if(!shm_manager_->isTopicExist(topic_, event)) {
    //   std::cout << "addPubTopic: " << topic_ << " " << event << std::endl;
    //   shm_manager_->addPubTopic(topic_, event);
    // }
    // // 触发事件：通知 ShmManager 更新 event_flag_ 并唤醒等待的订阅者
    // if (shm_manager_ && !topic_name_for_event_.empty()) {
    //   std::cout << "triggerEvent: " << topic_name_for_event_ << " " << event
    //   << "message: " << data.serialize() << std::endl;
    //   shm_manager_->triggerEvent(topic_name_for_event_, event);
    // }

    delete[] buffer;
    return 0;
  }

  // 设置 ShmManager 引用（由 Node 调用）
  void setShmManager(ShmManager* shm_manager) { shm_manager_ = shm_manager; }

  // 设置用于事件触发的 topic 名称（去除前缀的原始名称）
  void setTopicNameForEvent(const std::string& topic_name) {
    topic_name_for_event_ = topic_name;
  }
  int asyncService(const std::string& topic, const std::string& event,
                   const MsgT& data, int depth = 10);
  static int publish(const std::string& topic, const std::string& event,
                     const MsgT& data, int depth = 10) {
    size_t msg_serialize_size;
    msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
    uint8_t* buffer = new uint8_t[msg_serialize_size];
    Serializer::serialize<MsgT>(data, buffer, msg_serialize_size);
    std::string topic_str = topic + "_" + event;
    std::shared_ptr<ShmBase> shm =
        std::make_shared<ShmBase>(topic_str, msg_serialize_size);
    shm->Create();
    shm->Open();
    shm->Write(buffer, msg_serialize_size);
    delete[] buffer;
    return 0;
  }
  static int32_t postEvent(const std::string& topic, const std::string& event,
                           const MsgT& data, int depth = 10);
  static int32_t syncService(const std::string& topic, const std::string& event,
                             const MsgT& data, int depth = 10);
  void setHostId(int host) { host_id_ = host; };

  std::string getTopicName() const { return topic_; }

 private:
  // int count;
  int32_t host_id_;

  std::string topic_;

  std::shared_ptr<ShmBase> shm_;

  ShmManager* shm_manager_;           // ShmManager 引用，用于触发事件
  std::string topic_name_for_event_;  // 用于事件触发的 topic 名称（去除前缀）

  long long time_stamp_ = 0;
};