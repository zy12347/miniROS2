#pragma once
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"
#include <iostream>
#include <memory>
#include <string>
#include <sys/eventfd.h>

class PublisherBase {
public:
  virtual ~PublisherBase() = default; // 虚析构函数
  // 可添加通用接口（如关闭发布者）
};

class Node;
#define POST_EVENT(...) Publisher::Publish(__VA_ARGS__)

template <typename MsgT> class Publisher : public PublisherBase {
  friend class Node;

public:
  Publisher(std::string &topic) : topic_(topic){};

  Publisher(std::string &topic, int host_id)
      : topic_(topic), host_id_(host_id){};
  // Publisher(const Publisher &) = delete;
  // Publisher &operator=(const Publisher &) = delete;
  // Publisher(Publisher &&) = delete;
  // Publisher &operator=(Publisher &&) = delete;

  ~Publisher() = default;
  int publish(const std::string &event, const MsgT &data, int depth = 10) {

    size_t msg_serialize_size;
    msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
    std::cout << "msg_serialize_size: " << msg_serialize_size << std::endl;
    uint8_t *buffer = new uint8_t[msg_serialize_size];
    Serializer::serialize<MsgT>(data, buffer, msg_serialize_size);
    std::string topic_str = topic_ + "_" + event;
    if (shm_ == nullptr) {
      shm_ = std::make_shared<ShmBase>(topic_str, msg_serialize_size);
      shm_->Create();
    }
    // std::cout << data_str << std::endl;
    // shm_->Open();
    shm_->Write(buffer, msg_serialize_size);
    // std::string event_fd_path = "/tmp" + shm_->getShmName() + "_eventfd";
    // int sub_efd = open(event_fd_path.c_str(), O_WRONLY);
    // if (sub_efd == -1) {
    //   perror("open sub efd failed");
    //   return -1;
    // }
    // uint64_t notify = 1;
    // write(sub_efd, &notify, sizeof(notify));
    // close(sub_efd);
    delete[] buffer;
    return 0;
  }
  int asyncService(const std::string &topic, const std::string &event,
                   const MsgT &data, int depth = 10);
  static int publish(const std::string &topic, const std::string &event,
                     const MsgT &data, int depth = 10) {
    size_t msg_serialize_size;
    msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
    uint8_t *buffer = new uint8_t[msg_serialize_size];
    Serializer::serialize<MsgT>(data, buffer, msg_serialize_size);
    std::string topic_str = topic + "_" + event;
    std::shared_ptr<ShmBase> shm =
        std::make_shared<ShmBase>(topic_str, msg_serialize_size);
    shm->Create();
    shm->Write(buffer, msg_serialize_size);
    delete[] buffer;
    return 0;
  }
  static int32_t postEvent(const std::string &topic, const std::string &event,
                           const MsgT &data, int depth = 10);
  static int32_t syncService(const std::string &topic, const std::string &event,
                             const MsgT &data, int depth = 10);
  void setHostId(int host) { host_id_ = host; };

  std::string getTopicName() const { return topic_; }

private:
  // int count;
  int32_t host_id_;

  std::string topic_;

  std::shared_ptr<ShmBase> shm_;

  long long time_stamp_ = 0;
};