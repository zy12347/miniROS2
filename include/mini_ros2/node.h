/**
 * @file node.h
 * @brief miniROS2 节点类定义
 * @author miniROS2 Team
 * @date 2024
 */

#pragma once
#include <unistd.h>

#include <atomic>
#include <bitset>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mini_ros2/communication/event_manager.h"
#include "mini_ros2/communication/shm_manager.h"
#include "mini_ros2/logger.h"
#include "mini_ros2/pubsub/publisher.h"
#include "mini_ros2/pubsub/subscriber.h"
#include "mini_ros2/qos_policy.h"
#include "mini_ros2/service/client.h"
#include "mini_ros2/service/service.h"
#include "mini_ros2/thread_pool.h"
#include "mini_ros2/timer.h"

/**
 * @class Node
 * @brief miniROS2 节点类，是通信框架的核心入口点
 * 
 * Node 类提供了创建发布者、订阅者、服务和客户端的能力，并管理事件循环。
 * 每个 Node 实例代表一个独立的进程或线程，可以与其他 Node 实例进行通信。
 * 
 * @note Node 使用共享内存进行进程间通信，支持发布-订阅和服务-客户端两种模式
 * 
 * @example examples/basic/talker.cpp
 * @example examples/basic/listener.cpp
 * 
 * @code
 * // 创建节点
 * Node node("my_node");
 * 
 * // 创建发布者
 * auto pub = node.createPublisher<JsonValue>("chatter");
 * 
 * // 发布消息
 * JsonValue msg;
 * msg["data"] = "Hello, miniROS2!";
 * pub->publish("message", msg);
 * 
 * // 创建订阅者
 * auto sub = node.createSubscriber<JsonValue>("chatter", "message",
 *     [](const JsonValue& msg) {
 *         std::cout << "Received: " << msg.serialize() << std::endl;
 *     });
 * 
 * // 启动事件循环
 * node.spin();
 * @endcode
 */
class Node {
 public:
    /**
     * @brief 构造函数（移动语义版本）
     * @param node_name 节点名称，必须唯一
     * @param name_space 命名空间，用于隔离不同应用的话题和服务
     * @param domain_id 域ID，用于多域隔离（默认0）
     */
    Node(const std::string&& node_name, const std::string&& name_space = "", int domain_id = 0);
    
    /**
     * @brief 构造函数（引用版本）
     * @param node_name 节点名称，必须唯一
     * @param name_space 命名空间，用于隔离不同应用的话题和服务
     * @param domain_id 域ID，用于多域隔离（默认0）
     */
    Node(const std::string& node_name, const std::string& name_space = "", int domain_id = 0);

    void setNodeId(int node_id) { node_id_ = node_id; };
    
    /**
     * @brief 析构函数
     * 自动清理所有资源，包括停止事件循环、注销节点等
     */
    ~Node();

    /**
     * @brief 关闭节点
     * 停止事件循环并清理资源
     */
    void shutDown();
    
    /**
     * @brief 启动事件循环（阻塞）
     * 在主线程中启动事件循环，处理订阅消息、服务请求和定时器
     * 
     * @note 此方法会阻塞直到 stop() 被调用
     * 
     * @code
     * Node node("my_node");
     * // ... 创建发布者、订阅者等
     * node.spin();  // 阻塞，直到 node.stop() 被调用
     * @endcode
     */
    void spin();
    
    /**
     * @brief 停止事件循环
     * 通知事件循环退出，通常在信号处理函数中调用
     * 
     * @code
     * void signalHandler(int sig) {
     *     node->stop();  // 停止事件循环
     * }
     * @endcode
     */
    void stop();
    
    /**
     * @brief 检查节点是否处于活动状态
     * @return true 如果事件循环正在运行，false 否则
     */
    bool isActive() { return spinning_; };
    // -------------------------- Publisher 相关 --------------------------
    /**
     * @brief 创建发布者
     * @tparam MsgT 消息类型，必须支持 serialize() 方法
     * @param topic_name 话题名称
     * @param qos_policy QoS 策略，控制消息的可靠性和历史深度
     * @return 发布者智能指针
     * 
     * @note 消息类型必须实现 serialize() 方法，用于序列化
     * 
     * @example examples/basic/talker.cpp
     * @code
     * auto pub = node.createPublisher<JsonValue>("chatter");
     * JsonValue msg;
     * msg["data"] = "Hello";
     * pub->publish("message", msg);
     * @endcode
     */
    template <typename MsgT>
    std::shared_ptr<Publisher<MsgT>> createPublisher(const std::string& topic_name,
                                                     QosPolicy qos_policy = QosPolicy()) {
        // 生成完整话题路径（结合命名空间，避免冲突）
        std::string full_topic = shm_prefix_ + topic_name;

        // 创建具体Publisher实例（假设Publisher构造函数需要话题名和QoS深度）
        auto pub = std::make_shared<Publisher<MsgT>>(full_topic, qos_policy);

        // 设置原始 topic 名称，用于触发事件
        pub->setTopicNameForEvent(full_topic);   // 传递原始 topic 名称（不含前缀）

        // 线程安全地加入容器（基类指针转换）
        std::lock_guard<std::mutex> lock(node_mutex_);
        publishers_.push_back(pub);   // 自动转换为std::shared_ptr<PublisherBase>
        pub_topics_.push_back(full_topic);
        LOGD("create publisher " << full_topic);
        return pub;
    }

    // -------------------------- Subscriber 相关 --------------------------
    /**
     * @brief 创建订阅者
     * @tparam MsgT 消息类型，必须支持 deserialize() 方法
     * @param topic_name 话题名称
     * @param event_name 事件名称，用于区分同一话题下的不同事件类型
     * @param callback 消息回调函数，当收到消息时被调用
     * @param qos_policy QoS 策略，控制消息的可靠性和历史深度
     * @return 订阅者智能指针
     * 
     * @note 消息类型必须实现 deserialize() 方法，用于反序列化
     * 
     * @example examples/basic/listener.cpp
     * @code
     * auto sub = node.createSubscriber<JsonValue>("chatter", "message",
     *     [](const JsonValue& msg) {
     *         std::cout << "Received: " << msg.serialize() << std::endl;
     *     });
     * @endcode
     */
    template <typename MsgT>
    std::shared_ptr<Subscriber<MsgT>> createSubscriber(const std::string& topic_name, const std::string& event_name,
                                                       std::function<void(const MsgT&)> callback,
                                                       QosPolicy qos_policy = QosPolicy()) {
        std::string full_topic = shm_prefix_ + topic_name;

        // 创建具体Subscriber实例（调用私有构造函数，依赖友元关系）
        auto sub = std::make_shared<Subscriber<MsgT>>(full_topic, qos_policy);
        // sub->SetCallback(callback); // 设置回调
        sub->subscribe(event_name, callback);
        // 若需要QoS深度，可补充sub->SetQosDepth(qos_depth);

        // 线程安全地加入容器
        std::lock_guard<std::mutex> lock(node_mutex_);
        subscriptions_.push_back(sub);   // 自动转换为std::shared_ptr<SubscriberBase>
        sub_topics_.push_back(topic_name);
        SHM_MANAGER->addSubTopic(full_topic, event_name);

        // 注册 topic+event 组合，获取 event_id（订阅者订阅的是 pub 事件）
        int event_id = SHM_MANAGER->registerTopicEvent(full_topic, event_name, true);
        LOGD("event_id: " << event_id);
        // 存储订阅者索引到 event_id 的映射（用于在 spinLoop 中映射）
        if (event_id >= 0) {
            subscription_event_ids_.push_back(event_id);
        } else {
            subscription_event_ids_.push_back(-1);   // 标记失败
        }

        // EventSource ev;
        // sub->getEventSrc(ev);
        // event_manager_.addEventSource(ev);
        return sub;
    }

    /**
     * @brief 创建定时器
     * @param period 定时器周期（毫秒）
     * @param callback 定时器回调函数，周期性地被调用
     * 
     * @example
     * @code
     * node.createTimer(1000, []() {
     *     std::cout << "Timer fired!" << std::endl;
     * });  // 每秒触发一次
     * @endcode
     */
    void createTimer(uint64_t period, std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(node_mutex_);
        auto timer = std::make_shared<Timer>(period, callback);
        LOGD("createTimer: " << period);
        timers_.push_back(timer);
        min_timer_period_ = std::min(min_timer_period_, period);
    }

    template <typename MsgT>
    std::shared_ptr<Service<MsgT>> createService(const std::string& topic, const std::string& event,
                                                 std::function<void(MsgT& data)> callback,
                                                 QosPolicy qos_policy = QosPolicy()) {
        std::lock_guard<std::mutex> lock(node_mutex_);
        std::string full_topic = shm_prefix_ + topic;
        auto service           = std::make_shared<Service<MsgT>>(full_topic, event, callback, qos_policy);
        LOGD("createService: " << topic << " " << event);
        services_.push_back(service);
        service_topics_.push_back(topic);
        SHM_MANAGER->addSyncTopic(full_topic, event);
        int event_id = SHM_MANAGER->registerTopicEvent(full_topic, event, false);
        LOGD("event_id: " << event_id);
        if (event_id >= 0) {
            service_event_ids_.push_back(event_id);
        } else {
            service_event_ids_.push_back(-1);   // 标记失败
        }
        return service;
    }

    template <typename MsgT>
    std::shared_ptr<ClientRequest<MsgT>> createClient(const std::string& topic, const std::string& event,
                                                      std::function<void(const MsgT& data)> callback,
                                                      QosPolicy qos_policy = QosPolicy()) {
        std::lock_guard<std::mutex> lock(node_mutex_);
        std::string full_topic = shm_prefix_ + topic;
        // LOGD("full_topic: " << full_topic);
        auto client = std::make_shared<ClientRequest<MsgT>>(full_topic, event, qos_policy);
        LOGD("createClient: " << full_topic << " " << event);
        client->setTopicNameForEvent(full_topic);
        clients_.push_back(client);
        return client;
    }
    void printRegistry();

 private:
    void registerNode();
    void unregisterNode();
    void heartbeatLoop();
    void spinLoop();
    static void signalHandler(int signum);
    std::vector<std::shared_ptr<PublisherBase>> publishers_;
    std::vector<std::shared_ptr<SubscriberBase>> subscriptions_;
    std::vector<std::shared_ptr<ServiceBase>> services_;
    std::vector<std::shared_ptr<ClientRequestBase>> clients_;

    std::vector<std::string> pub_topics_;       // 发布的话题列表
    std::vector<std::string> sub_topics_;       // 订阅的话题列表
    std::vector<std::string> service_topics_;   // 服务的话题列表
    std::vector<int> subscription_event_ids_;   // 订阅者索引到 event_id 的映射
    std::vector<int> service_event_ids_;        // 服务者索引到 event_id 的映射

    std::thread heartbeat_thread_;
    std::atomic<bool> heartbeat_running_{false};   // 心跳机制，定时更新节点状态
    const int HEARTBEAT_INTERVAL         = 1;       // 秒
    const int HEARTBEAT_TIMEOUT          = 3;       // 秒

    EventManager event_manager_;   // 事件管理器
    std::thread spin_thread_;
    std::atomic<bool> spinning_{false};
    std::condition_variable spin_cv_;   // spin循环条件变量 事件处理循环

    std::vector<std::shared_ptr<Timer>> timers_;
    std::vector<std::function<void()>> callbacks_;

    static Node* signal_handler_node_;   // 信号处理节点
    // Private members for managing publishers and subscriptions
    int domain_id_;
    // 节点信息
    std::string node_name_;
    uint32_t node_id_ = 0;
    std::string name_space_;
    std::mutex node_mutex_;
    std::mutex callback_mutex_;
    std::string shm_prefix_;
    uint64_t min_timer_period_ = INT_MAX;

    std::shared_ptr<ThreadPool> thread_pool_;
};