#pragma once
#include "mini_ros2/communication/event_manager.h"
#include "mini_ros2/communication/shm_manager.h"
#include "mini_ros2/pubsub/publisher.h"
#include "mini_ros2/pubsub/subscriber.h"
#include "mini_ros2/timer.h"
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
class Node {
public:
  Node(const std::string &&node_name, const std::string &&name_space = "",
       int domain_id = 0);
  Node(const std::string &node_name, const std::string &name_space = "",
       int domain_id = 0);
  ~Node();

  void shutDown();
  void spin();
  void stop();
  bool isActive() { return spinning_; };
  // -------------------------- Publisher 相关 --------------------------
  template <typename MsgT>
  std::shared_ptr<Publisher<MsgT>>
  createPublisher(const std::string &topic_name, size_t qos_depth = 10) {
    // 生成完整话题路径（结合命名空间，避免冲突）
    std::string full_topic = shm_prefix_ + topic_name;

    // 创建具体Publisher实例（假设Publisher构造函数需要话题名和QoS深度）
    auto pub = std::make_shared<Publisher<MsgT>>(full_topic, qos_depth);
    
    // 设置 ShmManager 引用和原始 topic 名称，用于触发事件
    pub->setShmManager(shm_manager_.get());
    pub->setTopicNameForEvent(full_topic); // 传递原始 topic 名称（不含前缀）

    // 线程安全地加入容器（基类指针转换）
    std::lock_guard<std::mutex> lock(node_mutex_);
    publishers_.push_back(pub); // 自动转换为std::shared_ptr<PublisherBase>
    pub_topics_.push_back(full_topic);
    return pub;
  }

  // -------------------------- Subscriber 相关 --------------------------
  /**
   * @brief 创建 Subscriber（唯一入口）
   * @tparam MsgT 消息类型
   * @param topic_name 话题名
   * @param callback 消息回调函数（收到消息时触发）
   * @param qos 简化QoS（缓存深度，默认10）
   * @return 共享指针形式的 Subscriber
   */
  template <typename MsgT>
  std::shared_ptr<Subscriber<MsgT>>
  createSubscriber(const std::string &topic_name, const std::string &event_name,
                   std::function<void(const MsgT &)> callback,
                   size_t qos_depth = 10) {
    std::string full_topic = shm_prefix_ + topic_name;

    // 创建具体Subscriber实例（调用私有构造函数，依赖友元关系）
    auto sub = std::make_shared<Subscriber<MsgT>>(full_topic);
    // sub->SetCallback(callback); // 设置回调
    sub->subscribe(event_name, callback);
    // 若需要QoS深度，可补充sub->SetQosDepth(qos_depth);

    // 线程安全地加入容器
    std::lock_guard<std::mutex> lock(node_mutex_);
    subscriptions_.push_back(sub); // 自动转换为std::shared_ptr<SubscriberBase>
    sub_topics_.push_back(topic_name);
    shm_manager_->addSubTopic(full_topic, event_name);
    
    // 注册 topic+event 组合，获取 event_id（使用原始 topic 名称，不含前缀）
    int event_id = shm_manager_->registerTopicEvent(full_topic, event_name);
    std::cout << "event_id: " << event_id << std::endl;
    // 存储订阅者索引到 event_id 的映射（用于在 spinLoop 中映射）
    if (event_id >= 0) {
      subscription_event_ids_.push_back(event_id);
    } else {
      subscription_event_ids_.push_back(-1); // 标记失败
    }
    
    // EventSource ev;
    // sub->getEventSrc(ev);
    // event_manager_.addEventSource(ev);
    return sub;
  }

  void createTimer(std::chrono::milliseconds period,
                   std::function<void()> callback);

  void printRegistry();

private:
  void registerNode();
  void unregisterNode();
  void heartbeatLoop();
  void spinLoop();
  static void signalHandler(int signum);
  std::vector<std::shared_ptr<PublisherBase>> publishers_;
  std::vector<std::shared_ptr<SubscriberBase>> subscriptions_;

  std::shared_ptr<ShmManager> shm_manager_ =
      nullptr; //共享内存管理器,管理节点状态,节点发现,节点注册等

  std::vector<std::string> pub_topics_; //发布的话题列表
  std::vector<std::string> sub_topics_; //订阅的话题列表
  std::vector<int> subscription_event_ids_; // 订阅者索引到 event_id 的映射

  std::thread heartbeat_thread_;
  std::atomic<bool> heartbeat_running_ = false; // 心跳机制，定时更新节点状态
  const int HEARTBEAT_INTERVAL = 1;             // 秒
  const int HEARTBEAT_TIMEOUT = 3;              // 秒

  EventManager event_manager_; // 事件管理器
  std::thread spin_thread_;
  std::atomic<bool> spinning_ = false;
  std::condition_variable spin_cv_; // spin循环条件变量 事件处理循环

  std::vector<std::shared_ptr<Timer>> timers_;
  std::vector<std::function<void()>> callbacks_;

  static Node *signal_handler_node_; //信号处理节点
  // Private members for managing publishers and subscriptions
  int domain_id_;
  // 节点信息
  std::string node_name_;
  uint32_t node_id_ = 0;
  std::string name_space_;
  std::mutex node_mutex_;
  std::mutex callback_mutex_;
  std::string shm_prefix_;
};