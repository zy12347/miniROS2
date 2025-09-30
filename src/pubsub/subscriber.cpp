#include "mini_ros2/pubsub/subscriber.h"
#include <iostream>
Subscriber::~Subscriber(){
    is_running_ = false;
    if(listen_thread_.joinable()) {
        listen_thread_.join();
    }
}

void Subscriber::Subscribe(const std::string& event,std::function<void(const JsonValue& data)> callback) {
    SetCallback(callback);
    if(is_running_) {
        return;
    }
    if(shm_ == nullptr) {
        shm_ = std::make_shared<ShmBase>(topic_ + event,1024);
    }
    shm_->Open();
    is_running_ = true;
    listen_thread_ = std::thread(&Subscriber::ListenLoop, this);
}

void Subscriber::Subscribe(const std::string& event) {
    Subscribe(event, [](const JsonValue& data) {
        std::cout << data.serialize() << std::endl;
    });
}

void Subscriber::ListenLoop() {
    while(is_running_) {
        char data[1024];
        shm_->Read(data, 1024);
        JsonValue json = JsonValue::deserialize(data);
        callback_(json);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Subscriber::SetCallback(std::function<void(const JsonValue& data)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}
