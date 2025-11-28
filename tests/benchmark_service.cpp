/**
 * @file benchmark_service.cpp
 * @brief Service/Client 性能压测
 * @author miniROS2 Team
 * @date 2024
 * 
 * 测试指标：
 * - 吞吐量：每秒处理的请求数（req/s）
 * - 延迟：请求-响应的往返时间（RTT, us）
 * - 并发性能
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
struct ServicePerformanceStats {
    uint64_t total_requests = 0;
    uint64_t total_success = 0;
    uint64_t total_failed = 0;
    uint64_t total_latency_us = 0;
    uint64_t min_latency_us = UINT64_MAX;
    uint64_t max_latency_us = 0;
    std::vector<uint64_t> latencies;
    
    void addLatency(uint64_t latency_us, bool success) {
        total_requests++;
        if (success) {
            total_success++;
            total_latency_us += latency_us;
            min_latency_us = std::min(min_latency_us, latency_us);
            max_latency_us = std::max(max_latency_us, latency_us);
            latencies.push_back(latency_us);
        } else {
            total_failed++;
        }
    }
    
    void printStats(const std::string& test_name) {
        std::cout << "\n========== " << test_name << " ==========" << std::endl;
        std::cout << "Total requests: " << total_requests << std::endl;
        std::cout << "Success: " << total_success << std::endl;
        std::cout << "Failed:  " << total_failed << std::endl;
        
        if (total_success == 0) {
            std::cout << "No successful requests" << std::endl;
            std::cout << "==========================================\n" << std::endl;
            return;
        }
        
        double avg_latency = static_cast<double>(total_latency_us) / total_success;
        
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

/**
 * @brief 服务吞吐量测试
 */
void benchmark_service_throughput(int request_count) {
    std::cout << "\n========== Service Throughput Test ==========" << std::endl;
    std::cout << "Request count: " << request_count << std::endl;
    
    // 创建服务端节点
    Node service_node("service_node");
    auto service = service_node.createService<JsonValue>(
        "benchmark_service", "request",
        [](JsonValue& request) {
            // 简单的服务：计算 a + b
            int a = request["a"].asInt();
            int b = request["b"].asInt();
            request["sum"] = a + b;
        }
    );
    
    // 启动服务端（在后台线程）
    std::thread service_thread([&service_node]() {
        service_node.spin();
    });
    
    // 等待服务端就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 创建客户端节点
    Node client_node("client_node");
    auto client = client_node.createClient<JsonValue>("benchmark_service", "request", [](const JsonValue&) {});
    client->setTopicNameForEvent("benchmark_service");
    
    // 开始计时
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 发送请求
    uint64_t success_count = 0;
    for (int i = 0; i < request_count; i++) {
        JsonValue request;
        request["a"] = i;
        request["b"] = i + 1;
        
        JsonValue response;
        int ret = client->syncService("request", request, response, 5000);
        if (ret == 0) {
            success_count++;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    
    service_node.stop();
    service_thread.join();
    
    // 计算吞吐量
    double throughput = (static_cast<double>(success_count) * 1000000.0) / duration;
    
    std::cout << "Total requests: " << request_count << std::endl;
    std::cout << "Success: " << success_count << std::endl;
    std::cout << "Duration: " << duration / 1000.0 << " ms" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
              << throughput << " req/s" << std::endl;
    std::cout << "==========================================\n" << std::endl;
}

/**
 * @brief 服务延迟测试
 */
void benchmark_service_latency(int request_count) {
    std::cout << "\n========== Service Latency Test ==========" << std::endl;
    std::cout << "Request count: " << request_count << std::endl;
    
    ServicePerformanceStats stats;
    
    // 创建服务端节点
    Node service_node("service_node_latency");
    auto service = service_node.createService<JsonValue>(
        "latency_service", "request",
        [](JsonValue& request) {
            // 简单的服务：计算 a + b
            int a = request["a"].asInt();
            int b = request["b"].asInt();
            request["sum"] = a + b;
        }
    );
    
    // 启动服务端（在后台线程）
    std::thread service_thread([&service_node]() {
        service_node.spin();
    });
    
    // 等待服务端就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 创建客户端节点
    Node client_node("client_node_latency");
    auto client = client_node.createClient<JsonValue>("latency_service", "request", [](const JsonValue&) {});
    client->setTopicNameForEvent("latency_service");
    
    // 发送请求并测量延迟
    for (int i = 0; i < request_count; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        
        JsonValue request;
        request["a"] = i;
        request["b"] = i + 1;
        
        JsonValue response;
        int ret = client->syncService("request", request, response, 5000);
        
        auto end = std::chrono::high_resolution_clock::now();
        uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        
        stats.addLatency(latency, ret == 0);
        
        // 小延迟，避免过载
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    service_node.stop();
    service_thread.join();
    
    // 打印统计信息
    stats.printStats("Service Latency Test");
}

/**
 * @brief 并发客户端测试
 */
void benchmark_service_concurrent(int request_count, int concurrent_clients) {
    std::cout << "\n========== Concurrent Clients Test ==========" << std::endl;
    std::cout << "Request count per client: " << request_count << std::endl;
    std::cout << "Concurrent clients: " << concurrent_clients << std::endl;
    
    // 创建服务端节点
    Node service_node("service_node_concurrent");
    auto service = service_node.createService<JsonValue>(
        "concurrent_service", "request",
        [](JsonValue& request) {
            int a = request["a"].asInt();
            int b = request["b"].asInt();
            request["sum"] = a + b;
        }
    );
    
    // 启动服务端（在后台线程）
    std::thread service_thread([&service_node]() {
        service_node.spin();
    });
    
    // 等待服务端就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 创建多个客户端线程
    std::vector<std::thread> client_threads;
    std::atomic<uint64_t> total_success(0);
    std::atomic<uint64_t> total_failed(0);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int client_id = 0; client_id < concurrent_clients; client_id++) {
        client_threads.emplace_back([client_id, request_count, &total_success, &total_failed]() {
            Node client_node("client_node_" + std::to_string(client_id));
            auto client = client_node.createClient<JsonValue>(
                "concurrent_service", "request", [](const JsonValue&) {});
            client->setTopicNameForEvent("concurrent_service");
            
            for (int i = 0; i < request_count; i++) {
                JsonValue request;
                request["a"] = client_id * 1000 + i;
                request["b"] = client_id * 1000 + i + 1;
                
                JsonValue response;
                int ret = client->syncService("request", request, response, 5000);
                if (ret == 0) {
                    total_success++;
                } else {
                    total_failed++;
                }
            }
        });
    }
    
    // 等待所有客户端完成
    for (auto& thread : client_threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    
    service_node.stop();
    service_thread.join();
    
    // 计算吞吐量
    double throughput = (static_cast<double>(total_success) * 1000000.0) / duration;
    
    std::cout << "Total requests: " << (request_count * concurrent_clients) << std::endl;
    std::cout << "Success: " << total_success << std::endl;
    std::cout << "Failed:  " << total_failed << std::endl;
    std::cout << "Duration: " << duration / 1000.0 << " ms" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
              << throughput << " req/s" << std::endl;
    std::cout << "==========================================\n" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "miniROS2 Service Performance Benchmark" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 清理共享内存
    system("rm -rf /dev/shm/*miniros2* 2>/dev/null");
    
    // 吞吐量测试
    benchmark_service_throughput(1000);    // 1000个请求
    benchmark_service_throughput(5000);    // 5000个请求
    
    // 延迟测试
    benchmark_service_latency(500);        // 500个请求的延迟测试
    
    // 并发测试
    benchmark_service_concurrent(100, 1);   // 1个客户端，每个100请求
    benchmark_service_concurrent(100, 5);   // 5个并发客户端
    benchmark_service_concurrent(100, 10);  // 10个并发客户端
    
    std::cout << "All service benchmarks completed!" << std::endl;
    return 0;
}

