/**
 * @file publisher.h
 * @brief 发布者类定义
 * @author miniROS2 Team
 * @date 2024
 */

#pragma once
#include <sys/eventfd.h>

#include <iostream>
#include <memory>
#include <string>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/communication/shm_manager.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"
#include "mini_ros2/message/buffer_pool.h"
#include "mini_ros2/qos_policy.h"

/**
 * @class PublisherBase
 * @brief 发布者基类
 * 提供发布者的通用接口，用于类型擦除
 */
class PublisherBase {
 public:
    virtual ~PublisherBase() = default;   // 虚析构函数
                                          // 可添加通用接口（如关闭发布者）
};

class Node;
class ShmManager;
#define POST_EVENT(...) Publisher::Publish(__VA_ARGS__)

/**
 * @class Publisher
 * @brief 发布者类，用于向话题发布消息
 * @tparam MsgT 消息类型，必须支持 serialize() 方法
 * 
 * Publisher 使用共享内存将消息发送给订阅者。
 * 支持 QoS 策略，可以控制消息的可靠性和历史深度。
 * 
 * @note 通常通过 Node::createPublisher() 创建，不建议直接实例化
 * 
 * @example examples/basic/talker.cpp
 * @code
 * auto pub = node.createPublisher<JsonValue>("chatter");
 * JsonValue msg;
 * msg["data"] = "Hello, miniROS2!";
 * pub->publish("message", msg);
 * @endcode
 */
template <typename MsgT>
class Publisher : public PublisherBase {
    friend class Node;

 public:
    Publisher(const std::string& topic, QosPolicy qos_policy = QosPolicy()) 
        : topic_(topic), qos_policy_(qos_policy), buffer_pool_(std::unique_ptr<mini_ros2::BufferPool>(new mini_ros2::BufferPool())) {};

    /**
     * @brief 析构函数
     */
    ~Publisher() = default;
    
    /**
     * @brief 发布消息到指定事件
     * @param event 事件名称，用于区分同一话题下的不同事件类型
     * @param data 要发布的消息数据
     * @return 0 表示成功，非0 表示失败
     * 
     * @note 消息会被序列化并写入共享内存，然后触发事件通知订阅者
     * 
     * @example
     * @code
     * JsonValue msg;
     * msg["data"] = "Hello";
     * pub->publish("message", msg);
     * @endcode
     */
    int publish(const std::string& event, const MsgT& data) {
        size_t msg_serialize_size;
        msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
        // std::cout << "msg_serialize_size: " << msg_serialize_size << std::endl;
        
        // 使用缓冲区池复用内存，避免每次分配
        uint8_t* buffer = buffer_pool_->acquire(msg_serialize_size);
        Serializer::serialize<MsgT>(data, buffer, msg_serialize_size);
        std::string topic_str = topic_ + "_" + event;
        if (shm_ == nullptr) {
            // 使用一个足够大的固定大小，避免后续消息超出限制
            // 或者使用当前消息大小的 2 倍作为缓冲
            size_t initial_size = std::max(msg_serialize_size * 2, size_t(1024));
            shm_ = std::make_shared<ShmBase>(topic_str, initial_size, qos_policy_);
            if (shm_->Exists()) {
                shm_->Open();
            } else {
                shm_->Create();
                shm_->Open();
            }
        } else {
            // 检查消息大小是否超过限制
            size_t max_size = shm_->getMaxMsgSize();
            if (msg_serialize_size > max_size) {
                LOGE("消息大小超过限制: " << msg_serialize_size << " > " << max_size 
                     << ", 需要重新创建更大的共享内存");
                // 重新创建更大的共享内存
                shm_.reset();
                size_t new_size = std::max(msg_serialize_size * 2, size_t(1024));
                shm_ = std::make_shared<ShmBase>(topic_str, new_size, qos_policy_);
                shm_->Create();
                shm_->Open();
            }
        }
        // std::cout << data_str << std::endl;
        // shm_->Open();
        if (!SHM_MANAGER->isTopicExist(topic_, event)) {
            LOGD("addPubTopic: " << topic_ << " " << event);
            SHM_MANAGER->addPubTopic(topic_, event);
        }
        shm_->Write(buffer, msg_serialize_size);
        // 触发事件：通知 ShmManager 更新 event_flag_ 并唤醒等待的订阅者
        if (!topic_name_for_event_.empty()) {
            LOGD("triggerEvent: " << topic_name_for_event_ << " " << event << " message: " << data.serialize());
            SHM_MANAGER->triggerEvent(topic_name_for_event_, event);
        }

        // 不需要 delete[]，缓冲区由 buffer_pool_ 管理
        return 0;
    }

    // 设置用于事件触发的 topic 名称（去除前缀的原始名称）
    void setTopicNameForEvent(const std::string& topic_name) { topic_name_for_event_ = topic_name; }
    static int publish(const std::string& topic, const std::string& event, const MsgT& data, int depth = 10) {
        size_t msg_serialize_size;
        msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
        // 静态方法使用临时缓冲区（无法使用对象级缓冲区池）
        std::unique_ptr<uint8_t[]> buffer(new uint8_t[msg_serialize_size]);
        Serializer::serialize<MsgT>(data, buffer.get(), msg_serialize_size);
        std::string topic_str        = topic + "_" + event;
        std::shared_ptr<ShmBase> shm = std::make_shared<ShmBase>(topic_str, msg_serialize_size);
        shm->Create();
        shm->Open();
        shm->Write(buffer.get(), msg_serialize_size);
        // buffer 自动释放（RAII）
        return 0;
    }
    static int32_t postEvent(const std::string& topic, const std::string& event, const MsgT& data, int depth = 10);
    void setHostId(int host) { host_id_ = host; };

    std::string getTopicName() const { return topic_; }

 private:
    // int count;
    int32_t host_id_;

    std::string topic_;

    QosPolicy qos_policy_;

    std::shared_ptr<ShmBase> shm_;

    std::string topic_name_for_event_;   // 用于事件触发的 topic 名称（去除前缀）

    long long time_stamp_ = 0;
    
    std::unique_ptr<mini_ros2::BufferPool> buffer_pool_;  // 缓冲区池，用于复用内存
};