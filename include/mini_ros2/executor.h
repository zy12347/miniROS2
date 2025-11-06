#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

// 任务类型：无参数无返回值的函数（封装回调）
using Task = std::function<void()>;

// Executor基类
class Executor {
public:
  Executor() : running_(false) {}
  virtual ~Executor() { stop(); }

  // 启动Executor（纯虚函数，由子类实现线程逻辑）
  virtual void start() = 0;

  // 停止Executor
  void stop() {
    running_ = false;
    cv_.notify_all(); // 唤醒所有等待的线程
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  // 提交任务到队列
  void add_task(Task task) {
    std::lock_guard<std::mutex> lock(mtx_);
    tasks_.push(std::move(task));
    cv_.notify_one(); // 通知线程有新任务
  }

protected:
  std::queue<Task> tasks_;     // 任务队列
  std::mutex mtx_;             // 保护任务队列的互斥锁
  std::condition_variable cv_; // 通知线程有新任务
  std::atomic<bool> running_;  // 运行标志
  std::thread thread_;         // Executor线程（单线程用）
};

// 单线程Executor：所有任务串行执行
class SingleThreadedExecutor : public Executor {
public:
  void start() override {
    running_ = true;
    // 启动线程：循环取任务执行
    thread_ = std::thread([this]() {
      while (running_) {
        Task task;
        // 等待任务（无任务时阻塞）
        {
          std::unique_lock<std::mutex> lock(mtx_);
          cv_.wait(lock, [this]() { return !running_ || !tasks_.empty(); });
          if (!running_)
            break; // 退出标志
          task = std::move(tasks_.front());
          tasks_.pop();
        }
        // 执行任务（回调函数）
        task();
      }
    });
  }
};