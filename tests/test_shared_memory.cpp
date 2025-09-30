// #include "shared_memory.h"
#include "mini_ros2/communication/shm_base.h"
#include "time.h"
#include <iostream>
#include <unistd.h>
int main() {
  ShmBase shm("/test_sem", 1024);
  shm.Create();
  while (true) {
    uint64_t cur_time = static_cast<uint64_t>(time(nullptr));
    std::string message_str =
        "Hello from process 1 " + std::to_string(cur_time);
    const char *message = message_str.c_str(); // 从 std::string 取 C 风格指
    shm.Write(message, strlen(message) + 1);   // 包括终止符
    std::cout << "Sent: " << message << std::endl;
    sleep(1); // 每秒写入一次
  }
  return 0;
}