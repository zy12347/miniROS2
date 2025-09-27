#include "shared_memory.h"
#include <iostream>
#include <unistd.h>
int main() {
  SharedMemory shm("test_shm", 1024 * 1024); // 1MB
  if (shm.open()) {
    char *data = static_cast<char *>(shm.data());
    std::cout << "Data read from shared memory: " << data << std::endl;
    shm.close();
  } else {
    std::cout << "Failed to open shared memory." << std::endl;
  }
  return 0;
}