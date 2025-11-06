#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
 public:
    ThreadPool(int num_threads) {
        stop_ = false;
        std::cout << "ThreadPool constructor: " << num_threads << std::endl;
        for (int i = 0; i < num_threads; i++) {
            workers_.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() { return !stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    lock.unlock();
                    task();
                }
            });
        };
    }
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
            cv_.notify_all();
        }
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        stop_ = false;
    }

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    void stop() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
            cv_.notify_all();
        }
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    // 重载版本：直接接受 std::function<void()>
    void enqueue(std::function<void()> task) {
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.emplace(std::move(task));   // 任务入队
        lock.unlock();      // 解锁
        cv_.notify_one();   // 唤醒一个等待的线程处理任务
    }

    // 模板版本：接受函数和参数
    template<typename F, typename... Args> void enqueue(F&& f, Args&&... args) {
        // 将任务包装为std::function<void()>（绑定参数）
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // 加锁将任务加入队列
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.emplace(std::move(task));   // 任务入队

        lock.unlock();      // 解锁
        cv_.notify_one();   // 唤醒一个等待的线程处理任务
    }

 private:
    std::vector<std::thread> workers_;          // 工作线程列表
    std::queue<std::function<void()>> tasks_;   // 任务队列
    std::mutex mutex_;                          // 保护任务队列的互斥锁
    std::condition_variable cv_;                // 线程间通知的条件变量
    std::atomic<bool> stop_;                    // 线程池停止标志（原子变量保证线程安全）
};