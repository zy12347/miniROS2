#pragma once
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include <string>
#include <memory>

#define POST_EVENT(...) Publisher::Publish(__VA_ARGS__)
class Publisher {
 public:
    Publisher(std::string& topic) : topic_(topic){};

    Publisher(std::string& topic, int host_id) : topic_(topic), host_id_(host_id){};
    ~Publisher() = default;
    int Publish(const std::string& event, const JsonValue& data, int depth);
    int AsyncService(const std::string& topic, const std::string& event, const JsonValue& data, int depth = 1);
    static int Publish(const std::string& topic, const std::string& event, const JsonValue& data, int depth = 1);
    static int32_t PostEvent(const std::string& topic, const std::string& event, const JsonValue& data, int depth = 1);
    static int32_t SyncService(const std::string& topic, const std::string& event, const JsonValue& data, int depth = 1);
    void SetHostId(int host) { host_id_ = host; };

 private:
    // int count;
    int32_t host_id_;

    std::string topic_;

    std::shared_ptr<ShmBase> shm_;

    long long time_stamp_ = 0;
};