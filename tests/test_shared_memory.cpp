// #include "shared_memory.h"
#include <unistd.h>

#include <iostream>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include "mini_ros2/node.h"
#include "mini_ros2/pubsub/publisher.h"
#include "time.h"

int main() {
  Node node("test_node2");
  auto pub = node.createPublisher<JsonValue>("test");
  auto pub1 = node.createPublisher<JsonValue>("test");
  // node.printRegistry();
  node.createTimer(1000, [&pub, &pub1]() {
    JsonValue json;
    json["name"] = "John";
    json["age"] = 30;
    json["city"] = "New York";
    json["is_student"] = true;
    json["height"] = 1.8;
    json["weight"] = 70;
    json["time"] = std::to_string(static_cast<uint64_t>(time(nullptr)));

    JsonValue json1;
    json1["name"] = "Bod";
    json1["age"] = 10;
    json1["city"] = "LA";
    json1["is_student"] = false;
    json1["height"] = 1.2;
    json1["weight"] = 40;
    json1["time"] = std::to_string(static_cast<uint64_t>(time(nullptr)));
    std::cout << "pub test" << std::endl;
    pub->publish("test27", json);
    pub1->publish("test28", json1);
  });
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