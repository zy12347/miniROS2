#pragma once
#include <sys/eventfd.h>

#include <iostream>
#include <memory>
#include <string>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/communication/shm_manager.h"
#include "mini_ros2/message/message_serializer.h"
#include "mini_ros2/message/qos_buffer.h"

class ClientRequestBase {
 public:
    virtual ~ClientRequestBase() = default;   // 虚析构函数
                                              // 可添加通用接口（如关闭发布者）
};

class Node;
class ShmManager;
//    #define POST_EVENT(...) Publisher::Publish(__VA_ARGS__)

template <typename MsgT>
class ClientRequest : public ClientRequestBase {
    friend class Node;

 public:
    ClientRequest(const std::string& topic, const std::string& event) : topic_(topic), event_(event) {};

    ~ClientRequest() = default;
    void setTopic(const std::string& topic);

    void setEvent(const std::string& event);

    void setTopicNameForEvent(const std::string& topic_name) { topic_name_for_event_ = topic_name; }

    const MsgT& getResponse() const {
        return response_;
    }

    int syncService(const std::string& event, const MsgT& data, const MsgT& response,
                           int timeout = 100, int depth = 10) {
        size_t msg_serialize_size;
        msg_serialize_size = Serializer::getSerializedSize<MsgT>(data);
        uint8_t* buffer    = new uint8_t[msg_serialize_size];
        Serializer::serialize<MsgT>(data, buffer, msg_serialize_size);
        std::string topic_str = topic_ + "_" + event;
        if (shm_service_ == nullptr) {
            shm_service_ = std::make_shared<ShmBase>(topic_str, msg_serialize_size);
            shm_service_->Create();
            shm_service_->Open();
        }
        if (!SHM_MANAGER->isTopicExist(topic_, event)) {
            LOGD("addSyncTopic: " << topic_ << " " << event);
            SHM_MANAGER->addSyncTopic(topic_, event);
        }
        if (!topic_name_for_event_.empty()) {
            LOGD("triggerEvent: " << topic_name_for_event_ << " " << event << " message: " << data.serialize());
            SHM_MANAGER->triggerEvent(topic_name_for_event_, event);
        }
        shm_service_->Write(buffer, msg_serialize_size);
        delete[] buffer;
        while(true){
            std::bitset<EVENT_MAX_COUNT> trigger_event = SHM_MANAGER->waitForEventResponse(timeout);
            if (trigger_event.any()) {
                LOGD("waitForEventResponse success");
                int event_id = SHM_MANAGER->getTopicEventId(topic_, event);
                if(trigger_event[event_id]) {
                    getMessage();
                    LOGD("service " << topic_ << " " << event << " response: " << response_.serialize());
                }
                return 0;
            }
            break;
        }
        return -1;
    }

    int32_t asyncService(const std::string& topic, const std::string& event, const MsgT& data,
                                const MsgT& response, int depth = 10);

    void getMessage() {
        try {
            if (shm_service_ == nullptr) {
                // 订阅时创建共享内存
                std::string shm_name = topic_ + "_" + event_;
                shm_service_                 = std::make_shared<ShmBase>(shm_name);
                shm_service_->Open();
                LOGD("create shm_name: " << shm_name << " event: " << event_ << " for service");
                // link("/proc/self/fd/" + std::to_string(efd), eventfd_path_.c_str());
            }
            size_t msg_serialize_size = shm_service_->getDataSize();
            LOGD("msg_serialize_size: " << msg_serialize_size);
            uint8_t* data = new uint8_t[msg_serialize_size];
            shm_service_->ReadUnlocked(data, msg_serialize_size);
            Serializer::deserialize<MsgT>(data, msg_serialize_size, response_);
            delete[] data;
        } catch (const std::exception& e) {
            LOGE("service listen error: " << e.what());
        }
    }

 private:
    std::string topic_;
    std::string event_;
    std::string topic_name_for_event_;
    std::shared_ptr<ShmBase> shm_service_;
    MsgT response_;
    // std::shared_ptr<ShmBase> shm_res_;
};