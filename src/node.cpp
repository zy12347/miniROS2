#include "mini_ros2/node.h"
Node *Node::signal_handler_node_ = nullptr;

// 信号处理函数
void Node::signalHandler(int signum) {
    if (signal_handler_node_) {
        std::cout << "\nReceived signal " << signum << ", stopping node..." << std::endl;
        signal_handler_node_->stop();
    }
}
Node::Node(const std::string &node_name, const std::string &name_space, int domain_id)
    : node_name_(node_name),
      name_space_(name_space),
      domain_id_(domain_id) {
    // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
    std::string name = name_space.empty() ? name_space_ : name_space + "_";
    shm_prefix_      = "/" + std::to_string(domain_id_) + "_" + name;
    shm_manager_     = std::make_shared<ShmManager>();
    signal(SIGINT, Node::signalHandler);
    signal(SIGTERM, Node::signalHandler);
    signal_handler_node_ = this;
    registerNode();
}

Node::Node(const std::string &&node_name, const std::string &&name_space, int domain_id)
    : node_name_(node_name),
      name_space_(name_space),
      domain_id_(domain_id) {
    // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
    std::string name = name_space.empty() ? name_space_ : name_space + "_";
    shm_prefix_      = "/" + std::to_string(domain_id_) + "_" + name;
    shm_manager_     = std::make_shared<ShmManager>();
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
    new_node.pid            = getpid();
    new_node.last_heartbeat = time(nullptr);
    new_node.is_alive       = true;

    // 填充发布话题
    new_node.pub_topic_count = std::min((int)pub_topics_.size(), MAX_TOPICS_PER_NODE);
    for (int i = 0; i < new_node.pub_topic_count; i++) {
        std::strcpy(new_node.pub_topics[i], pub_topics_[i].c_str());
    }

    // 填充订阅话题
    new_node.sub_topic_count = std::min((int)sub_topics_.size(), MAX_TOPICS_PER_NODE);
    for (int i = 0; i < new_node.sub_topic_count; i++) {
        std::strcpy(new_node.sub_topics[i], sub_topics_[i].c_str());
    }

    // 更新活跃节点计数
    shm_manager_->addNode(new_node);

    std::cout << "Node registered: " << node_name_ << " (ID: " << node_id_ << ")" << std::endl;
    return;
}

void Node::unregisterNode() {
    if (!shm_manager_)
        return;
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

void Node::createTimer(std::chrono::milliseconds period, std::function<void()> callback) {
    auto timer = std::make_shared<Timer>(period, callback);
    timers_.push_back(timer);
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
    while (spinning_) {
      // std::cout << "spinLoop" << std::endl;
        // ✅ 先获取锁，再等待条件变量（符合 POSIX 规范）
        // 注意：pthread_cond_wait() 会在等待时自动释放锁，被唤醒时重新获取锁
        std::cout << "shmManagerLock" << std::endl;
        shm_manager_->shmManagerLock();
        std::cout << "shmManagerWait" << std::endl;
        shm_manager_->shmManagerWait(); // 等待条件变量通知（等待期间锁被释放）
        std::cout << "getTriggerEventUnlocked" << std::endl;
        // std::cout << "shmManagerWait" << std::endl;
        // 读取事件标志位（此时已重新持有锁）
        shm_manager_->readTopicsInfoUnlocked();
        int trigger_event = shm_manager_->getTriggerEventUnlocked();
        std::cout << "getTriggerEvent: " << trigger_event << std::endl;
        // ✅ 在持有锁的情况下，收集需要处理的订阅者索引和对应的 event_id
        std::vector<size_t> subscribers_to_execute;
        std::vector<int> event_ids_to_clear;
        {
            std::lock_guard<std::mutex> lock(node_mutex_); // 保护 subscriptions_ 访问
            for (size_t id = 0; id < subscriptions_.size() && id < subscription_event_ids_.size(); id++) {
                int event_id = subscription_event_ids_[id];
                std::cout << "event_id: " << event_id << std::endl;
                if (event_id >= 0 && event_id < 32) { // 假设使用 32 位整数
                    // 检查对应的位是否被设置
                    if (trigger_event & (1 << event_id)) {
                        std::cout << "trigger_event: " << trigger_event << std::endl;
                        subscribers_to_execute.push_back(id);
                        event_ids_to_clear.push_back(event_id);
                    }
                }
            }
        }
        
        // ✅ 批量清除事件标志位（在持有锁的情况下，原子操作）
        // 注意：此时已经持有 ShmManager 锁，readAndClearEventFlags 不会再加锁
        if (!event_ids_to_clear.empty()) {
            std::cout << "readAndClearEventFlags: " << event_ids_to_clear.size() << std::endl;
            shm_manager_->readAndClearEventFlagsUnlocked(event_ids_to_clear);
        }
        
        // 解锁 ShmManager（允许其他线程更新注册表或触发事件）
        shm_manager_->shmManagerUnlock();
        
        // ✅ 在无锁状态下执行回调（避免长时间持有锁，防止阻塞注册表更新）
        std::lock_guard<std::mutex> lock(node_mutex_); // 保护 subscriptions_ 访问
        std::cout << "subscribers_to_execute: " << subscribers_to_execute.size() << std::endl;
        for (size_t id : subscribers_to_execute) {
            std::cout << "execute: " << id << std::endl;
            if (id < subscriptions_.size()) {
                subscriptions_[id]->execute();
            }
            std::cout << "execute end: " << id << std::endl;
        }
    }
}

// 启动Spin循环
void Node::spin() {
    if (spinning_)
        return;

    spinning_    = true;
    spin_thread_ = std::thread(&Node::spinLoop, this);

    // 等待spin线程结束
    if (spin_thread_.joinable()) {
        spin_thread_.join();
    }
}

// 停止Spin循环
void Node::stop() {
    spinning_ = false;
    spin_cv_.notify_one();   // 唤醒等待的线程

    if (spin_thread_.joinable()) {
        spin_thread_.join();
    }
}

void Node::printRegistry() { shm_manager_->printRegistry(); }