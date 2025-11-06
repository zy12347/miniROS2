#pragma once
#include <atomic>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

typedef enum {
  EVENT_TYPE_SUB, // 订阅者事件（接收消息）
  EVENT_TYPE_PUB  // 发布者事件（如发送确认，可选）
} EventType;

typedef struct {
  std::string efd_name;
  int efd;                        // eventfd 文件描述符
  EventType type;                 // 事件类型
  void *data;                     // 业务数据（如 SubInfo 或 PubInfo）
  std::function<void()> callback; // 事件触发时的回调函数
} EventSource;

class EventManager {
public:
  EventManager() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
      std::cerr << "epoll_create1 failed" << std::endl;
      return;
    }
    thread_ = std::thread(&EventManager::run, this);
  };
  ~EventManager() {
    stop();
    if (epoll_fd_ != -1) {
      close(epoll_fd_);
    }
    std::lock_guard<std::mutex> lock(epoll_mutex_);
    for (const auto &src : event_sources_) {
      if (src.efd != -1) {
        close(src.efd);
      }
    }
  };
  bool addEventSource(const EventSource &source) {
    if (epoll_fd_ == -1 || !source.callback) {
      return false;
    }

    int flags = fcntl(source.efd, F_GETFL, 0);
    if (fcntl(source.efd, F_SETFL, flags | O_NONBLOCK) == -1) {
      perror("Failed to set eventfd non-blocking");
      return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = &event_sources_.back();
    std::lock_guard<std::mutex> lock(epoll_mutex_);
    event_sources_.push_back(source);
    ev.data.ptr = &event_sources_.back();
    // 注册到 epoll
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, source.efd, &ev) == -1) {
      perror("Failed to add event to epoll");
      event_sources_.pop_back(); // 回滚
      return false;
    }
    return true;
  };
  bool removeEventSource(int efd) {
    std::lock_guard<std::mutex> lock(epoll_mutex_);
    for (auto it = event_sources_.begin(); it != event_sources_.end(); ++it) {
      if (it->efd == efd) {
        // 从 epoll 中移除
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, efd, nullptr) == -1) {
          perror("Failed to remove event from epoll");
          return false;
        }
        // 关闭 eventfd 并从列表中删除
        close(it->efd);
        event_sources_.erase(it);
        return true;
      }
    }
    return false;
  };

  void stop() {
    running_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
  };

private:
  void run() {
    while (running_) {
      int num_ready = epoll_wait(epoll_fd_, events_, MAX_EVENTS, 1000);
      if (num_ready == -1) {
        if (errno == EINTR) {
          continue;
        }
        std::cerr << "epoll_wait failed" << std::endl;
        break;
      }
      // 处理所有就绪事件
      for (int i = 0; i < num_ready; ++i) {
        // 从 epoll_event.data 中获取对应的 EventSource
        EventSource *src = static_cast<EventSource *>(events_[i].data.ptr);
        if (!src) {
          continue;
        }

        // 检查事件类型（仅处理可读事件）
        if (events_[i].events & EPOLLIN) {
          // 读取 eventfd 数据（重置事件，避免持续触发）
          uint64_t notify;
          ssize_t n = read(src->efd, &notify, sizeof(notify));
          if (n != sizeof(notify)) {
            perror("Failed to read eventfd");
            continue;
          }

          // 调用事件对应的回调函数（处理业务逻辑，如订阅者接收消息）
          if (src->callback) {
            src->callback(); // 传入关联的业务数据
          }
        }
      }
    }
  }
  std::atomic<bool> running_{true};
  std::thread thread_;
  int epoll_fd_ = -1;
  static const int MAX_EVENTS = 64; // 最大同时处理的就绪事件数
  std::mutex epoll_mutex_;          // 保护epoll_fd_的互斥锁
  epoll_event events_[MAX_EVENTS];  // 就绪事件数组
  std::vector<EventSource> event_sources_; // 事件源容器
  std::unordered_map<std::string, std::string> topic_eventfd_map_;
};