#include <unistd.h>

#include <iostream>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include "mini_ros2/node.h"
#include "mini_ros2/service/service.h"
#include "time.h"

int main() {
  pthread_setname_np(pthread_self(), "service_server");
  Node node("service_server_node");
  
  // 创建服务，处理请求并返回响应
  auto service = node.createService<JsonValue>("calculator", "add", [](JsonValue& msg) {
    LOGD("Service received request: " << msg.serialize());
    
    // 处理请求（这里是一个简单的加法服务）
    int a = 0, b = 0;
    if (msg.isMember("a")&&msg["a"].isInt()) {
      a = msg["a"].asInt();
    }
    if (msg.isMember("b")&&msg["b"].isInt()) {
      b = msg["b"].asInt();
    }
    msg = JsonValue();
    msg["result"] = a + b;
    msg["timestamp"] = std::to_string(static_cast<uint64_t>(time(nullptr)));
    LOGD("Service sending response: " << msg.serialize());
  });
  
  node.printRegistry();
  LOGD("Service server started, waiting for requests...");
  node.spin();
  return 0;
}

