#pragma once
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
class Subscriber{
public:
    Subscriber(std::string& topic):topic_(topic){};
    ~Subscriber();
    void Subscribe(const std::string& event);
    void Subscribe(const std::string& event,std::function<void(const JsonValue& data)> callback);
    void SetCallback(std::function<void(const JsonValue& data)> callback);
    void SetHostId(int host_id){host_id_ = host_id;};
    void ListenLoop();
private:
    std::mutex mutex_;
    std::string topic_;
    std::shared_ptr<ShmBase> shm_;
    std::function<void(const JsonValue& data)> callback_;
    int depth_;
    int host_id_;
    std::atomic<bool> is_running_ = false;
    std::thread listen_thread_;
    long long time_stamp_ = 0;
};