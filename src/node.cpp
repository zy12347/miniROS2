#include "mini_ros2/node.h"

#include <bitset>
Node* Node::signal_handler_node_ = nullptr;

// 信号处理函数
void Node::signalHandler(int signum) {
  if (signal_handler_node_) {
    std::cout << "\nReceived signal " << signum << ", stopping node..."
              << std::endl;
    std::string signame;
    switch (signum) {
      case SIGINT:
        signame = "SIGINT (Ctrl+C)";
        break;
      case SIGTSTP:
        signame = "SIGTSTP (Ctrl+Z)";
        break;
      case SIGTERM:
        signame = "SIGTERM (kill)";
        break;
      default:
        signame = "unknown signal";
    }
    std::cout << "Signal received,exiting normally to allow cleanup signame: "
              << signame << std::endl;
    signal_handler_node_->stop();
    // signal_handler_node_->unregisterNode();

    // 确保资源清理完成后再退出
    // 注意：不要直接调用 exit(0)，让程序正常退出以便析构函数被调用
    // 析构函数会自动清理共享内存
  }
}
Node::Node(const std::string& node_name, const std::string& name_space,
           int domain_id)
    : node_name_(node_name), name_space_(name_space), domain_id_(domain_id) {
  // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
  std::string name = name_space.empty() ? name_space_ : name_space + "_";
  shm_prefix_ = "/" + std::to_string(domain_id_) + "_" + name;
  shm_manager_ = std::make_shared<ShmManager>();
  signal(SIGINT, Node::signalHandler);
  signal(SIGTERM, Node::signalHandler);
  signal(SIGTSTP, Node::signalHandler);
  signal_handler_node_ = this;
  registerNode();
  thread_pool_ = std::make_shared<ThreadPool>(4);
  std::cout << "Node constructor: " << node_name_ << std::endl;
}

Node::Node(const std::string&& node_name, const std::string&& name_space,
           int domain_id)
    : node_name_(node_name), name_space_(name_space), domain_id_(domain_id) {
  // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
  std::string name = name_space.empty() ? name_space_ : name_space + "_";
  shm_prefix_ = "/" + std::to_string(domain_id_) + "_" + name;
  shm_manager_ = std::make_shared<ShmManager>();
  signal(SIGINT, Node::signalHandler);
  signal(SIGTERM, Node::signalHandler);
  signal(SIGTSTP, Node::signalHandler);
  signal_handler_node_ = this;
  registerNode();
  std::cout << "after registerNode" << std::endl;
  thread_pool_ = std::make_shared<ThreadPool>(4);
  std::cout << "Node constructor: " << node_name_ << std::endl;
}

// 注册节点
void Node::registerNode() {
  std::cout << "registerNode: " << node_name_ << std::endl;
  if (!shm_manager_) return;

  // 检查节点数量限制
  if (shm_manager_->getAliveNodeCount() >= MAX_NODE_COUNT) {
    std::cerr << "Maximum node count reached" << std::endl;
    return;
  }

  // 分配节点ID
  node_id_ = shm_manager_->getNextNodeId();
  shm_manager_->setNodeId(node_id_);
  if (node_id_ == -1) {
    std::cerr << "No free slot for new node" << std::endl;
    return;
  }
  // 找到空闲位置
  NodeInfo new_node;
  // 填充节点信息
  new_node.node_id = node_id_;
  std::strcpy(new_node.node_name, node_name_.c_str());
  new_node.pid = getpid();
  new_node.last_heartbeat = time(nullptr);
  new_node.is_alive = true;

  // 填充发布话题
  new_node.pub_topic_count =
      std::min((int)pub_topics_.size(), MAX_TOPICS_PER_NODE);
  for (int i = 0; i < new_node.pub_topic_count; i++) {
    std::strcpy(new_node.pub_topics[i], pub_topics_[i].c_str());
  }

  // 填充订阅话题
  new_node.sub_topic_count =
      std::min((int)sub_topics_.size(), MAX_TOPICS_PER_NODE);
  for (int i = 0; i < new_node.sub_topic_count; i++) {
    std::strcpy(new_node.sub_topics[i], sub_topics_[i].c_str());
  }

  // 更新活跃节点计数
  shm_manager_->addNode(new_node);

  std::cout << "Node registered: " << node_name_ << " (ID: " << node_id_ << ")"
            << std::endl;
  return;
}

void Node::unregisterNode() {
  if (!shm_manager_) return;
  std::cout << "unregisterNode: " << node_name_ << std::endl;
  // shm_manager_->alive_node_count--;
  shm_manager_->removeNode();
}

// 析构函数
Node::~Node() {
  // 停止心跳
  heartbeat_running_ = false;
  if (heartbeat_thread_.joinable()) {
    heartbeat_thread_.join();
  }
  // 停止线程池（必须在注销节点前，确保所有任务完成）
  if (thread_pool_) {
    thread_pool_->stop();
    thread_pool_ = nullptr;  // 释放线程池资源
  }

  // 注销节点
  unregisterNode();

  // 重置信号处理
  if (signal_handler_node_ == this) {
    signal_handler_node_ = nullptr;
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
  }
}

// 心跳循环
void Node::heartbeatLoop() {
  while (heartbeat_running_) {
    if (shm_manager_ && node_id_ != 0) {
      if (shm_manager_->isNodeAlive()) {
        shm_manager_->updateNodeHeartbeat();
        break;
      }
    }
    // 等待心跳间隔
    std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
  }
}

// Spin循环实现
void Node::spinLoop() {
  pthread_setname_np(pthread_self(), "spinloop");
  std::cout << "spinLoop started" << std::endl;
  while (spinning_) {
    // std::cout << "spinLoop" << std::endl;
    // 先获取共享内存锁，再等待条件变量（符合 POSIX 规范）
    // 注意：pthread_cond_wait()
    // 会在等待时自动释放共享内存锁，被唤醒时重新获取锁
    // 关键：在等待条件变量时，不持有 registry_mutex_，避免死锁
    // std::cout << "shmManagerLockShm" << std::endl;

    // 1. 等待事件通知（使用独立的事件通知共享内存，内部已管理锁）
    //    使用较短的超时时间，以便能够及时响应 spinning_ 标志的变化
    //    不再需要复杂的锁管理，事件通知和注册表已分离
    uint64_t timeout = std::min(min_timer_period_,
                                static_cast<uint64_t>(100));  // 最多等待100ms
    uint32_t trigger_event = shm_manager_->waitForEvent(timeout);

    // 检查是否应该退出（在等待期间 spinning_ 可能被设置为 false）
    if (!spinning_) {
      std::cout << "spinLoop: spinning_ is false, exiting loop" << std::endl;
      break;
    }

    // 2. 读取注册表信息（如果需要更新本地 topics_ 缓存）
    //    注意：注册表更新频率低，可以定期读取，不需要每次事件都读取
    shm_manager_->readTopicsInfo();

    // 3. 获取 registry_mutex_ 来访问本地 topics_ 数据（用于查找 event_id 对应的
    // topic）
    shm_manager_->shmManagerLockRegistry();
    int trigger_event_int = static_cast<int>(
        trigger_event);  // 在持有锁的情况下，收集需要处理的订阅者索引和对应的
                         // event_id
    if (trigger_event_int > 0) {
      //   std::cout << "getTriggerEvent: " << trigger_event_int
      //             << " (binary: " << std::bitset<32>(trigger_event_int) <<
      //             ")"
      //             << std::endl;
      std::vector<int> processed_event_ids;  // 记录已处理的事件ID
      {
        std::lock_guard<std::mutex> lock(
            node_mutex_);  // 保护 subscriptions_ 访问
        // std::cout << "Checking " << subscriptions_.size() << " subscriptions"
        //           << std::endl;
        for (size_t id = 0;
             id < subscriptions_.size() && id < subscription_event_ids_.size();
             id++) {
          int event_id = subscription_event_ids_[id];
          std::cout << "  subscription[" << id << "] event_id: " << event_id
                    << std::endl;
          if (event_id >= 0 && event_id < 32) {  // 假设使用 32 位整数
            // 检查对应的位是否被设置
            bool is_triggered = (trigger_event_int & (1 << event_id)) != 0;
            std::cout << "    bit " << event_id << " is "
                      << (is_triggered ? "SET" : "NOT SET") << std::endl;
            if (is_triggered) {
              std::cout << "    Processing event for subscription[" << id
                        << "] with event_id=" << event_id << std::endl;
              try {
                if (thread_pool_ && spinning_) {
                  // 使用捕获的 shared_ptr，不需要访问 Node 的成员
                  std::function<void()> task_func =
                      subscriptions_[id]
                          ->createTaskFromSubEvent();  // 拷贝数据并创建任务
                  thread_pool_->enqueue(std::move(task_func));
                  std::cout << "    Enqueued task for subscription[" << id
                            << "]" << std::endl;
                  // 记录已处理的事件ID
                  processed_event_ids.push_back(event_id);
                }
              } catch (const std::exception& e) {
                std::cerr << "Task exception: " << e.what() << std::endl;
              }
            }
          }
        }
      }

      // 只清除已处理的事件标志位，而不是全部清除
      // 这样可以避免清除其他进程/线程同时触发的事件
      for (int event_id : processed_event_ids) {
        shm_manager_->clearTriggerEvent(event_id);
        std::cout << "  Cleared event_id=" << event_id << std::endl;
      }
    }
    // 解锁 ShmManager（允许其他线程更新注册表或触发事件）
    shm_manager_->shmManagerUnlockRegistry();

    // std::cout << "timer" << std::endl;
    for (auto& timer : timers_) {
      if (timer->isReady()) {
        // std::cout << "ready" << std::endl;
        try {
          if (thread_pool_ && spinning_) {
            // 使用捕获的 shared_ptr，不需要访问 Node 的成员
            std::function<void()> task_func =
                timer->createTaskFromTimer();  // 拷贝数据并创建任务
            thread_pool_->enqueue(std::move(task_func));
            // std::cout << "enqueue" << std::endl;
          }
        } catch (const std::exception& e) {
          std::cerr << "Task exception: " << e.what() << std::endl;
        }
      }
    }
  }
  std::cout << "spinLoop ended" << std::endl;
  return;
}

// 启动Spin循环
void Node::spin() {
  if (spinning_) return;

  spinning_ = true;
  spin_thread_ = std::thread(&Node::spinLoop, this);

  // 等待spin线程结束
  if (spin_thread_.joinable()) {
    spin_thread_.join();
  }
}

// 停止Spin循环
void Node::stop() {
  //   std::cout << "Node::stop() called, setting spinning_ = false" <<
  //   std::endl;
  spinning_ = false;
  thread_pool_->stop();
  // 唤醒所有等待事件的线程，确保 spinLoop 能够及时退出
  // 这很重要，因为 waitForEvent() 可能在等待条件变量
  if (shm_manager_) {
    // std::cout << "Node::stop() calling notifyAllWaiters()" << std::endl;
    shm_manager_->notifyAllWaiters();
  }
  //   std::cout << "Node::stop() completed" << std::endl;
}

void Node::printRegistry() { shm_manager_->printRegistry(); }