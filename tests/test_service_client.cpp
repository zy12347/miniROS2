#include <unistd.h>

#include <iostream>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include "mini_ros2/node.h"
#include "mini_ros2/service/client.h"
#include "time.h"

int main() {
  pthread_setname_np(pthread_self(), "service_client");
  Node node("service_client_node");
  
  // 创建客户端（callback 参数可以为空，因为使用同步调用）
  auto client = node.createClient<JsonValue>("calculator", "add", [](const JsonValue&) {});
  
  // 设置用于触发事件的话题名称，通知服务端有新请求
  // client->setTopicNameForEvent("calculator");
  
  node.printRegistry();
  
  // 创建定时器，定期发送服务请求
  node.createTimer(10000, [&client]() {
    // 创建请求数据
    JsonValue request;
    request["a"] = 10;
    request["b"] = 20;
    request["timestamp"] = std::to_string(static_cast<uint64_t>(time(nullptr)));
    
    LOGD("Client sending request: " << request.serialize());
    
    // 创建响应对象（占位符，实际响应通过 getResponse() 获取）
    JsonValue response;
    
    // 调用同步服务
    int result = client->syncService("add", request, response, 2000);
    
    if (result == 0) {
      // 获取响应
      const JsonValue& actual_response = client->getResponse();
      LOGD("Client received response: " << actual_response.serialize());
      
      // 处理响应
      if (actual_response.isMember("result") && actual_response["result"].isInt()) {
        int sum = actual_response["result"].asInt();
        LOGD("Calculation result: " << sum);
      }
    } else {
      LOGD("Service call failed or timed out");
    }
  });
  
  LOGD("Service client started, sending requests every 3 seconds...");
  node.spin();
  return 0;
}

