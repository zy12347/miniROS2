// #include "shared_memory.h"
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include "mini_ros2/node.h"
#include "mini_ros2/pubsub/publisher.h"
#include "time.h"
#include <iostream>
#include <unistd.h>

int main() {
  Node node("test_node2");
  auto pub = node.createPublisher<JsonValue>("test");
  JsonValue json;
  json["name"] = "John";
  json["age"] = 30;
  json["city"] = "New York";
  json["is_student"] = true;
  json["height"] = 1.8;
  json["weight"] = 70;
  node.printRegistry();
  node.spin();
  return 0;
}
// ShmBase shm("/test_sem", 1024);
// shm.Create();
// while (true) {
//   uint64_t cur_time = static_cast<uint64_t>(time(nullptr));
//   JsonValue json;
//   json["name"] = "John";
//   json["age"] = 30;
//   json["city"] = "New York";
//   json["is_student"] = true;
//   json["height"] = 1.8;
//   json["weight"] = 70;
//   std::cout << json.serialize() << std::endl;
//   std::string json_str = json.serialize();
//   // std::string message_str =
//   //     "Hello from process 1 " + std::to_string(cur_time);
//   const char *message = json_str.c_str(); // 从 std::string 取 C 风格指
//   shm.Write(message, strlen(message) + 1);   // 包括终止符
//   std::cout << "Sent: " << message << std::endl;
//   sleep(1); // 每秒写入一次
// }
//   return 0;
// }