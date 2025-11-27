/**
 * @file talker_example.cpp
 * @brief 发布者示例程序
 * @author miniROS2 Team
 * @date 2024
 * 
 * 此示例演示如何创建节点、发布者，并发布消息到话题。
 * 
 * @code
 * // 编译运行
 * cd build
 * cmake ..
 * make
 * ./examples/talker_example
 * @endcode
 */

#include "mini_ros2/node.h"
#include "mini_ros2/message/json.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "time.h"

using namespace mini_ros2;

int main(int argc, char** argv) {
    // 创建节点
    Node node("talker_node");
    
    // 创建发布者，话题名为 "chatter"
    // 使用默认 QoS 策略（KEEP_LAST, history_depth=1）
    auto publisher = node.createPublisher<JsonValue>("chatter");
    
    // 设置用于事件触发的 topic 名称（可选）
    publisher->setTopicNameForEvent("chatter");
    
    int count = 0;
    
    // 创建定时器，每秒发布一次消息
    node.createTimer(1000, [&]() {
        // 创建消息
        JsonValue msg;
        msg["count"] = count++;
        msg["message"] = "Hello, miniROS2!";
        msg["timestamp"] = std::to_string(static_cast<uint64_t>(time(nullptr)));
        
        // 发布消息到 "message" 事件
        int ret = publisher->publish("message", msg);
        if (ret == 0) {
            std::cout << "Published: " << msg.serialize() << std::endl;
        } else {
            std::cerr << "Failed to publish message" << std::endl;
        }
    });
    
    std::cout << "Talker node started. Publishing messages every second..." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    
    // 启动事件循环（阻塞）
    node.spin();
    
    return 0;
}

