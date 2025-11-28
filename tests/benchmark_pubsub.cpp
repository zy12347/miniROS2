/**
 * @file benchmark_pubsub.cpp
 * @brief Publisher/Subscriber 性能压测
 * @author miniROS2 Team
 * @date 2024
 * 
 * 测试指标：
 * - 吞吐量：每秒处理的消息数（msg/s）
 * - 延迟：消息从发布到接收的时间（us）
 * - CPU 使用率
 */

#include "mini_ros2/node.h"
#include "mini_ros2/message/json.h"
#include "mini_ros2/qos_policy.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

using namespace mini_ros2;

// 性能统计结构
struct PerformanceStats {
    uint64_t total_messages = 0;
    uint64_t total_latency_us = 0;
    uint64_t min_latency_us = UINT64_MAX;
    uint64_t max_latency_us = 0;
    std::vector<uint64_t> latencies;
    
    void addLatency(uint64_t latency_us) {
        total_messages++;
        total_latency_us += latency_us;
        min_latency_us = std::min(min_latency_us, latency_us);
        max_latency_us = std::max(max_latency_us, latency_us);
        latencies.push_back(latency_us);
    }
    
    void printStats(const std::string& test_name) {
        if (total_messages == 0) {
            std::cout << test_name << ": No messages received" << std::endl;
            return;
        }
        
        double avg_latency = static_cast<double>(total_latency_us) / total_messages;
        
        // 计算中位数和百分位数
        std::sort(latencies.begin(), latencies.end());
        double median = latencies[latencies.size() / 2];
        double p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];
        double p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];
        
        // 计算标准差
        double variance = 0;
        for (uint64_t lat : latencies) {
            variance += (lat - avg_latency) * (lat - avg_latency);
        }
        double stddev = std::sqrt(variance / latencies.size());
        
        std::cout << "\n========== " << test_name << " ==========" << std::endl;
        std::cout << "Total messages: " << total_messages << std::endl;
        std::cout << "Latency (us):" << std::endl;
        std::cout << "  Min:    " << std::setw(10) << min_latency_us << " us" << std::endl;
        std::cout << "  Max:    " << std::setw(10) << max_latency_us << " us" << std::endl;
        std::cout << "  Avg:    " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << avg_latency << " us" << std::endl;
        std::cout << "  Median: " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << median << " us" << std::endl;
        std::cout << "  P95:    " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << p95 << " us" << std::endl;
        std::cout << "  P99:    " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << p99 << " us" << std::endl;
        std::cout << "  StdDev: " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << stddev << " us" << std::endl;
        std::cout << "==========================================\n" << std::endl;
    }
};

// 全局统计
std::atomic<uint64_t> g_received_count(0);
std::atomic<uint64_t> g_sent_count(0);
PerformanceStats g_stats;
std::atomic<bool> g_running(true);

/**
 * @brief 吞吐量测试：测量每秒能处理多少消息
 */
void benchmark_throughput(int message_count, int message_size) {
    std::cout << "\n========== Throughput Test ==========" << std::endl;
    std::cout << "Message count: " << message_count << std::endl;
    std::cout << "Message size: " << message_size << " bytes" << std::endl;
    
    g_received_count = 0;
    g_sent_count = 0;
    g_running = true;
    
    // 创建发布者节点
    QosPolicy qos_policy(QosPolicy::KEEP_ALL, 10);
    Node pub_node("pub_node");
    auto publisher = pub_node.createPublisher<JsonValue>("benchmark_topic", qos_policy);
    // publisher->setTopicNameForEvent("benchmark_topic");
    
    // 创建订阅者节点（在另一个线程）
    QosPolicy qos_policy_sub(QosPolicy::KEEP_ALL, 10);
    Node sub_node("sub_node");
    auto subscriber = sub_node.createSubscriber<JsonValue>(
        "benchmark_topic", "message",
        [message_size](const JsonValue& msg) {
                g_received_count++;
                LOGD("received message: " << msg.serialize());
            },
            qos_policy_sub
        );
    
    // 启动订阅者节点（在后台线程）
    std::thread sub_thread([&sub_node]() {
        sub_node.spin();
    });
    
    // 等待订阅者就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 创建测试消息
    JsonValue test_msg;
    std::string data_str(message_size, 'A');
    test_msg["data"] = data_str;
    test_msg["index"] = 0;
    
    // 开始计时
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 发布消息
    for (int i = 0; i < message_count; i++) {
        test_msg["index"] = i;
        publisher->publish("message", test_msg);
        g_sent_count++;
    }
    
    // 等待所有消息被接收
    int wait_count = 0;
    while (g_received_count < message_count && wait_count < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        wait_count++;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    
    g_running = false;
    sub_node.stop();
    sub_thread.join();
    
    // 计算吞吐量
    double throughput_msg_per_sec = (static_cast<double>(g_received_count) * 1000000.0) / duration;
    double throughput_mb_per_sec = (throughput_msg_per_sec * message_size) / (1024.0 * 1024.0);
    
    std::cout << "Sent:     " << g_sent_count << " messages" << std::endl;
    std::cout << "Received: " << g_received_count << " messages" << std::endl;
    std::cout << "Duration: " << duration / 1000.0 << " ms" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
              << throughput_msg_per_sec << " msg/s" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
              << throughput_mb_per_sec << " MB/s" << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

/**
 * @brief 延迟测试：测量消息从发布到接收的延迟
 */
void benchmark_latency(int message_count) {
    std::cout << "\n========== Latency Test ==========" << std::endl;
    std::cout << "Message count: " << message_count << std::endl;
    
    g_stats = PerformanceStats();
    g_running = true;
    
    // 创建发布者节点
    Node pub_node("pub_node_latency");
    auto publisher = pub_node.createPublisher<JsonValue>("latency_topic");
    publisher->setTopicNameForEvent("latency_topic");
    
    // 创建订阅者节点
    Node sub_node("sub_node_latency");
    auto subscriber = sub_node.createSubscriber<JsonValue>(
        "latency_topic", "message",
        [](const JsonValue& msg) {
            // 计算延迟
            // 注意：JsonValue 可能没有 asUInt64，使用 asInt 或 asString
            uint64_t send_time = 0;
            if (msg.isMember("timestamp")) {
                if (msg["timestamp"].isInt()) {
                    send_time = static_cast<uint64_t>(msg["timestamp"].asInt());
                } else if (msg["timestamp"].isString()) {
                    send_time = std::stoull(msg["timestamp"].asString());
                }
            }
            uint64_t recv_time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            uint64_t latency = recv_time - send_time;
            g_stats.addLatency(latency);
        }
    );
    
    // 启动订阅者节点（在后台线程）
    std::thread sub_thread([&sub_node]() {
        sub_node.spin();
    });
    
    // 等待订阅者就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 发布消息并测量延迟
    JsonValue test_msg;
    for (int i = 0; i < message_count; i++) {
        uint64_t send_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        test_msg["index"] = i;
        test_msg["timestamp"] = std::to_string(send_time);  // 使用字符串存储时间戳
        
        publisher->publish("message", test_msg);
        
        // 小延迟，避免过载
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // 等待所有消息被接收
    int wait_count = 0;
    while (g_stats.total_messages < message_count && wait_count < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        wait_count++;
    }
    
    g_running = false;
    sub_node.stop();
    sub_thread.join();
    
    // 打印统计信息
    g_stats.printStats("Latency Test");
}

/**
 * @brief 不同 QoS 策略的性能对比
 */
void benchmark_qos_comparison(int message_count) {
    std::cout << "\n========== QoS Policy Comparison ==========" << std::endl;
    
    // 测试 KEEP_LAST
    {
        std::cout << "\n--- KEEP_LAST (history_depth=10) ---" << std::endl;
        QosPolicy qos_keep_last(QosPolicy::KEEP_LAST, 10);
        
        Node pub_node("pub_keep_last");
        auto publisher = pub_node.createPublisher<JsonValue>("qos_topic", qos_keep_last);
        publisher->setTopicNameForEvent("qos_topic");
        
        Node sub_node("sub_keep_last");
        std::atomic<uint64_t> received(0);
        auto subscriber = sub_node.createSubscriber<JsonValue>(
            "qos_topic", "message",
            [&received](const JsonValue& msg) {
                received++;
            },
            qos_keep_last
        );
        
        std::thread sub_thread([&sub_node]() {
            sub_node.spin();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto start = std::chrono::high_resolution_clock::now();
        JsonValue msg;
        for (int i = 0; i < message_count; i++) {
            msg["index"] = i;
            publisher->publish("message", msg);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double throughput = (static_cast<double>(received) * 1000000.0) / duration;
        
        std::cout << "Received: " << received << " messages" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " msg/s" << std::endl;
        
        sub_node.stop();
        sub_thread.join();
    }
    
    // 测试 KEEP_ALL
    {
        std::cout << "\n--- KEEP_ALL (history_depth=10) ---" << std::endl;
        QosPolicy qos_keep_all(QosPolicy::KEEP_ALL, 10);
        
        Node pub_node("pub_keep_all");
        auto publisher = pub_node.createPublisher<JsonValue>("qos_topic_all", qos_keep_all);
        publisher->setTopicNameForEvent("qos_topic_all");
        
        Node sub_node("sub_keep_all");
        std::atomic<uint64_t> received(0);
        auto subscriber = sub_node.createSubscriber<JsonValue>(
            "qos_topic_all", "message",
            [&received](const JsonValue& msg) {
                received++;
            },
            qos_keep_all
        );
        
        std::thread sub_thread([&sub_node]() {
            sub_node.spin();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto start = std::chrono::high_resolution_clock::now();
        JsonValue msg;
        for (int i = 0; i < message_count; i++) {
            msg["index"] = i;
            publisher->publish("message", msg);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double throughput = (static_cast<double>(received) * 1000000.0) / duration;
        
        std::cout << "Received: " << received << " messages" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                  << throughput << " msg/s" << std::endl;
        
        sub_node.stop();
        sub_thread.join();
    }
    
    std::cout << "==========================================\n" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "miniROS2 Performance Benchmark Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 清理共享内存
    system("rm -rf /dev/shm/*miniros2* 2>/dev/null");
    
    // 吞吐量测试
    benchmark_throughput(100, 1);      // 1万条消息，每条100字节
    // benchmark_throughput(100, 1024);     // 1万条消息，每条1KB
    // benchmark_throughput(100, 10240);    // 1万条消息，每条10KB
    
    // 延迟测试
    // benchmark_latency(1000);               // 1000条消息的延迟测试
    
    // QoS 策略对比
    // benchmark_qos_comparison(1000);
    
    std::cout << "All benchmarks completed!" << std::endl;
    return 0;
}

