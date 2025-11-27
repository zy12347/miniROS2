/**
 * @file service_example.cpp
 * @brief 服务示例程序
 * @author miniROS2 Team
 * @date 2024
 * 
 * 此示例演示如何创建服务端和客户端，实现请求-响应通信。
 * 
 * @code
 * // 编译运行
 * cd build
 * cmake ..
 * make
 * 
 * // 终端1：运行服务端
 * ./examples/service_server
 * 
 * // 终端2：运行客户端
 * ./examples/service_client
 * @endcode
 */

#include "mini_ros2/node.h"
#include "mini_ros2/message/json.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace mini_ros2;

// 服务端示例
int service_server_main() {
    Node node("service_server");
    
    // 创建服务，处理 "add_two_ints" 请求
    auto service = node.createService<JsonValue>(
        "add_two_ints",                    // 服务名称
        "request",                         // 事件名称
        [](JsonValue& request) {           // 服务回调函数（可以修改 request 作为响应）
            std::cout << "Service received request: " << request.serialize() << std::endl;
            
            // 处理请求
            int a = request["a"].asInt();
            int b = request["b"].asInt();
            int sum = a + b;
            
            // 设置响应（修改 request 对象）
            request["sum"] = sum;
            request["status"] = "success";
            
            std::cout << "Service response: " << request.serialize() << std::endl;
        }
    );
    
    std::cout << "Service server started. Waiting for requests..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    
    node.spin();
    return 0;
}

// 客户端示例
int service_client_main() {
    Node node("service_client");
    
    // 创建客户端
    auto client = node.createClient<JsonValue>("add_two_ints", "request",[](const JsonValue&) {});
    client->setTopicNameForEvent("add_two_ints");
    
    // 创建请求
    JsonValue request;
    request["a"] = 10;
    request["b"] = 20;
    
    std::cout << "Sending request: " << request.serialize() << std::endl;
    
    // 调用服务（同步）
    JsonValue response;
    int ret = client->syncService("request", request, response, 5000);  // 5秒超时
    
    if (ret == 0) {
        std::cout << "Received response: " << response.serialize() << std::endl;
        if (response.isMember("sum")) {
            std::cout << "Sum: " << response["sum"].asInt() << std::endl;
        }
    } else {
        std::cerr << "Service call failed or timeout" << std::endl;
    }
    
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "client") {
        return service_client_main();
    } else {
        return service_server_main();
    }
}

