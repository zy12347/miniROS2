#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include <iostream>
#include <unistd.h>
int main() {
  ShmBase shm("/test_sem", 1024);
  shm.Open();
  while (true) {
    char data[1024];
    shm.Read(data, 1024); // 包括终止符
    JsonValue json = JsonValue::deserialize(data);
    std::cout << "Received: " << json.serialize() << std::endl;
    sleep(1); // 每秒读取一次
  }
  return 0;
}