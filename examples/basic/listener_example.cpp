/**
 * @file listener_example.cpp
 * @brief 订阅者示例程序
 * @author miniROS2 Team
 * @date 2024
 * 
 * 此示例演示如何创建节点、订阅者，并接收消息。
 * 
 * @code
 * // 编译运行
 * cd build
 * cmake ..
 * make
 * ./examples/listener_example
 * @endcode
 */

#include "mini_ros2/node.h"
#include "mini_ros2/message/json.h"
#include <iostream>

using namespace mini_ros2;

int main(int argc, char** argv) {
    // 创建节点
    Node node("listener_node");
    
    // 创建订阅者，订阅 "chatter" 话题的 "message" 事件
    // 使用默认 QoS 策略（KEEP_LAST, history_depth=1）
    auto subscriber = node.createSubscriber<JsonValue>(
        "chatter",                    // 话题名称
        "message",                    // 事件名称
        [](const JsonValue& msg) {    // 回调函数
            std::cout << "Received message: " << msg.serialize() << std::endl;
            
            // 可以访问消息的字段
            if (msg.isMember("count")) {
                std::cout << "  Count: " << msg["count"].asInt() << std::endl;
            }
            if (msg.isMember("message")) {
                std::cout << "  Message: " << msg["message"].asString() << std::endl;
            }
        }
    );
    
    std::cout << "Listener node started. Waiting for messages..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    
    // 启动事件循环（阻塞）
    node.spin();
    
    return 0;
}

