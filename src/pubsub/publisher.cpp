#include "mini_ros2/pubsub/publisher.h"
#include <iostream>

int Publisher::Publish(const std::string& topic, const std::string& event, const JsonValue& data, int depth) {
    std::string data_str  = data.serialize();
    std::string topic_str = topic + event;
    ShmBase shm(topic_str, depth);
    shm.Create();
    shm.Write(data_str.c_str(), data_str.size());
    return 0;
}

int Publisher::Publish(const std::string& event, const JsonValue& data, int depth) {
    std::string data_str  = data.serialize();
    std::string topic_str = topic_ + event;
    if (shm_ == nullptr) {
        shm_ = std::make_shared<ShmBase>(topic_str, data_str.size());
        shm_->Create();
    }
    std::cout << data_str << std::endl;
    // shm_->Open();
    shm_->Write(data_str.c_str(),data_str.size());
    return 0;
}