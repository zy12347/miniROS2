#include "mini_ros2/node.h"
Node *Node::signal_handler_node_ = nullptr;

// 信号处理函数
void Node::signalHandler(int signum) {
  if (signal_handler_node_) {
    std::cout << "\nReceived signal " << signum << ", stopping node..."
              << std::endl;
    signal_handler_node_->stop();
  }
}
Node::Node(const std::string &node_name, const std::string &name_space,
           int domain_id)
    : node_name_(node_name), name_space_(name_space), domain_id_(domain_id) {
  // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
  std::string name = name_space.empty() ? name_space_ : name_space + "_";
  shm_prefix_ = "/" + std::to_string(domain_id_) + "_" + name;
  shm_manager_ = std::make_shared<ShmManager>();
  signal(SIGINT, Node::signalHandler);
  signal(SIGTERM, Node::signalHandler);
  signal_handler_node_ = this;
  registerNode();
}

Node::Node(const std::string &&node_name, const std::string &&name_space,
           int domain_id)
    : node_name_(node_name), name_space_(name_space), domain_id_(domain_id) {
  // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
  std::string name = name_space.empty() ? name_space_ : name_space + "_";
  shm_prefix_ = "/" + std::to_string(domain_id_) + "_" + name;
  shm_manager_ = std::make_shared<ShmManager>();
  signal(SIGINT, Node::signalHandler);
  signal(SIGTERM, Node::signalHandler);
  signal_handler_node_ = this;
  registerNode();
}

// 注册节点
void Node::registerNode() {
  std::cout << "registerNode: " << node_name_ << std::endl;
  if (!shm_manager_)
    return;

  // 检查节点数量限制
  if (shm_manager_->getAliveNodeCount() >= MAX_NODE_COUNT) {
    std::cerr << "Maximum node count reached" << std::endl;
    return;
  }

  // 分配节点ID
  node_id_ = shm_manager_->getNextNodeId();
  if (node_id_ == -1) {
    std::cerr << "No free slot for new node" << std::endl;
    return;
  }
  // 找到空闲位置
  NodeInfo new_node;
  // 填充节点信息
  new_node.node_id = node_id_;
  new_node.node_name = node_name_;
  new_node.pid = getpid();
  new_node.last_heartbeat = time(nullptr);
  new_node.is_alive = true;

  // 填充发布话题
  new_node.pub_topic_count =
      std::min((int)pub_topics_.size(), MAX_TOPICS_PER_NODE);
  for (int i = 0; i < new_node.pub_topic_count; i++) {
    new_node.pub_topics[i] = pub_topics_[i];
  }

  // 填充订阅话题
  new_node.sub_topic_count =
      std::min((int)sub_topics_.size(), MAX_TOPICS_PER_NODE);
  for (int i = 0; i < new_node.sub_topic_count; i++) {
    new_node.sub_topics[i] = sub_topics_[i];
  }

  // 更新活跃节点计数
  shm_manager_->addNode(new_node);

  std::cout << "Node registered: " << node_name_ << " (ID: " << node_id_ << ")"
            << std::endl;
  return;
}

void Node::unregisterNode() {
  if (!shm_manager_)
    return;
  std::cout << "unregisterNode: " << node_name_ << std::endl;
  // shm_manager_->alive_node_count--;
  shm_manager_->removeNode(node_id_);
}

// 析构函数
Node::~Node() {
  // 停止心跳
  heartbeat_running_ = false;
  if (heartbeat_thread_.joinable()) {
    heartbeat_thread_.join();
  }

  // 停止spin循环
  stop();

  // 注销节点
  unregisterNode();

  // 重置信号处理
  if (signal_handler_node_ == this) {
    signal_handler_node_ = nullptr;
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
  }
}

void Node::createTimer(std::chrono::milliseconds period,
                       std::function<void()> callback) {
  auto timer = std::make_shared<Timer>(period, callback);
  timers_.push_back(timer);
}

// 心跳循环
void Node::heartbeatLoop() {
  while (heartbeat_running_) {
    if (shm_manager_ && node_id_ != 0) {
      if (shm_manager_->isNodeAlive(node_id_)) {
        shm_manager_->updateNodeHeartbeat(node_id_);
        break;
      }
    }
    // 等待心跳间隔
    std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
  }
}

// Spin循环实现
void Node::spinLoop() {
  while (spinning_) {
    // 处理定时器
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(callback_mutex_);

    for (auto &timer : timers_) {
      if (timer->isActive() &&
          now - timer->getTriggerTime() >= timer->getPeriod()) {
        timer->execute();
        timer->updateTriggerTime(now);
      }
    }

    // 处理回调函数
    if (!subscriptions_.empty()) {

      for (const auto &sub : subscriptions_) {
        sub->execute();
      }
    }

    // 等待新事件或超时
    if (subscriptions_.empty() && timers_.empty()) {
      // 没有回调或定时器，等待通知
      std::unique_lock<std::mutex> ulock(callback_mutex_);
      spin_cv_.wait_for(ulock, std::chrono::milliseconds(100));
    } else {
      // 有定时器，短暂休眠
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

// 启动Spin循环
void Node::spin() {
  if (spinning_)
    return;

  spinning_ = true;
  spin_thread_ = std::thread(&Node::spinLoop, this);

  // 等待spin线程结束
  if (spin_thread_.joinable()) {
    spin_thread_.join();
  }
}

// 停止Spin循环
void Node::stop() {
  spinning_ = false;
  spin_cv_.notify_one(); // 唤醒等待的线程

  if (spin_thread_.joinable()) {
    spin_thread_.join();
  }
}

void Node::printRegistry() { shm_manager_->printRegistry(); }