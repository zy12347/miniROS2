#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

std::unordered_map<std::string, std::string> node_registry; //节点->端口
std::mutex registry_mutex; // 用于保护 node_registry 的互斥锁

void handle_client(int client_socket) {
  char buffer[1024] = {0};
  read(client_socket, buffer, 1024);
  std::string msg(buffer);
  if (msg.substr(0, 8) == "REGISTER") {
    std::string node_name = msg.substr(9, msg.find(',') - 9);
    std::string node_addr = msg.substr(msg.find(',') + 1);
    {
      std::lock_guard<std::mutex> lock(registry_mutex);
      node_registry[node_name] = node_addr;
    }
    std::cout << "Registered: " << node_name << " at " << node_addr
              << std::endl;
    send(client_socket, "OK", 2, 0);
  }
  close(client_socket);
}
int main() {
  // 创建socket：AF_INET（IPv4），SOCK_DGRAM（TCP协议），0（默认协议）
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  // 定义服务器地址结构
  sockaddr_in address;
  address.sin_family = AF_INET;         // IPV4
  address.sin_addr.s_addr = INADDR_ANY; //监听所有地址
  address.sin_port = htons(8080);       // 端口号
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    close(server_fd);
    return EXIT_FAILURE;
  } // 绑定socket和地址
  if (listen(server_fd, 5) < 0) {
    perror("listen failed");
    close(server_fd);
    return EXIT_FAILURE;
  } // 监听连接（最多允许5个排队连接）
  std::cout << "Registry server started on port 8080" << std::endl;
  while (true) {
    int client_socket = accept(server_fd, nullptr, nullptr);
    std::thread(handle_client, client_socket).detach();
  }
  return 0;
}