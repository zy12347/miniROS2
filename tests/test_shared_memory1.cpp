#include <unistd.h>

#include <iostream>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include "mini_ros2/node.h"
#include "mini_ros2/pubsub/subscriber.h"
#include "mini_ros2/qos_policy.h"
int main() {
  pthread_setname_np(pthread_self(), "main2");
  Node Node("test_node2");
  QosPolicy qos_policy(QosPolicy::KEEP_ALL, 5);
  auto sub = Node.createSubscriber<JsonValue>(
      "test", "test", [](const JsonValue& data) {
        std::cout << "Received: " << data.serialize() << std::endl;
      }, qos_policy);  // topic event function
  auto sub1 = Node.createSubscriber<JsonValue>(
      "test", "test1", [](const JsonValue& data) {
        std::cout << "Received1: " << data.serialize() << std::endl;
      }, qos_policy);  // topic event function
  Node.printRegistry();
  Node.spin();
  return 0;
  // ShmBase shm("/test_sem", 1024);
  // shm.Open();
  // while (true) {
  //   char data[1024];
  //   shm.Read(data, 1024); // 包括终止符
  //   JsonValue json = JsonValue::deserialize(data);
  //   std::cout << "Received: " << json.serialize() << std::endl;
  //   sleep(1); // 每秒读取一次
  // }
  // return 0;
}