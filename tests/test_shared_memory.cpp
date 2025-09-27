#include "shared_memory.h"
#include <iostream>
#include <unistd.h>
int main() {
  SharedMemory shm("test_shm", 1024 * 1024); // 1MB
  if (shm.create()) {
    std::cout << "Shared memory created." << std::endl;
    char *data = static_cast<char *>(shm.data());
    snprintf(data, 1024, "Hello from shared memory!");
    std::cout << "Data written to shared memory: " << data << std::endl;
    std::cout << "等待读取进程...（30秒后退出）" << std::endl;
    sleep(30);
    shm.close();
    // shm.unlink(); // Uncomment to delete the shared memory
  } else {
    std::cout << "Failed to create shared memory." << std::endl;
  }
  return 0;
}