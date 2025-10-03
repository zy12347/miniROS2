// #include "mini_ros2/pubsub/subscriber.h"
// #include <iostream>

// template <typename MsgT>
// void Subscriber<MsgT>::Subscribe(
//     const std::string &event, std::function<void(const MsgT &data)> callback)
//     {
//   SetCallback(callback);
//   if (is_running_) {
//     return;
//   }
//   if (shm_ == nullptr) {
//     shm_ = std::make_shared<ShmBase>(topic_ + "/" + event, 1024);
//   }
//   shm_->Open();
//   is_running_ = true;
//   listen_thread_ = std::thread(&Subscriber<MsgT>::ListenLoop, this);
// }

// template <typename MsgT>
// void Subscriber<MsgT>::Subscribe(const std::string &event) {
//   Subscribe(event, [](const MsgT &data) {
//     std::cout << data.serialize() << std::endl;
//   });
// }

// template <typename MsgT> void Subscriber<MsgT>::ListenLoop() {
//   while (is_running_) {
//     char data[1024];
//     shm_->Read(data, 1024);
//     MsgT json = MsgT::deserialize(data);
//     callback_(json);
//     std::this_thread::sleep_for(std::chrono::milliseconds(10));
//   }
// }

// template <typename MsgT>
// void Subscriber<MsgT>::SetCallback(
//     std::function<void(const MsgT &data)> callback) {
//   std::lock_guard<std::mutex> lock(mutex_);
//   callback_ = callback;
// }
