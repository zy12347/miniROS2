#include <pthread.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "mini_ros2/communication/event_notification_shm.h"
#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"

#define MAX_TOPICS_PER_NODE EVENT_MAX_COUNT
#define MAX_NODE_COUNT 16
#define MAX_NODE_NAME_LEN 64
#define MAX_TOPIC_NAME_LEN 64
#define SHM_MANAGER_NAME "/miniros2_dds_shm_manager"
#define TOPIC_INFO_SIZE sizeof(TopicsInfo)
#define NODE_INFO_SIZE sizeof(NodesInfo)
#define MAX_SHM_MANGER_SIZE sizeof(ShmManagerInfo)

struct TopicInfo {
  char name_[MAX_TOPIC_NAME_LEN];
  int event_id_;
};

struct TopicsInfo {
  int topics_count;
  TopicInfo topics[MAX_TOPICS_PER_NODE];
};

struct NodeInfo {
  int node_id;
  int pid;
  int pub_topic_count;
  int sub_topic_count;
  bool is_alive;
  int last_heartbeat;
  char node_name[MAX_NODE_NAME_LEN];
};

struct NodesInfo {
  int nodes_count;
  int alive_node_count;
  NodeInfo nodes[MAX_NODE_COUNT];
};

struct ShmManagerInfo {
  TopicsInfo topic_info;
  NodesInfo nodes_info;
};

class ShmManager {
 public:
  // 去中心化构造函数：检查共享内存是否存在，不存在则创建
  ShmManager();

  // 析构函数：清理共享内存
  ~ShmManager();
  void addSubTopic(const std::string& topic_name,
                   const std::string& event_name);
  void addPubTopic(const std::string& topic_name,
                   const std::string& event_name);
  void removeSubTopic(const std::string& topic_name,
                      const std::string& event_name);
  void removePubTopic(const std::string& topic_name,
                      const std::string& event_name);
  void updateNodeHeartbeat();  // 更新指定id节点心跳
  bool isNodeAlive();          // 判断指定id节点是否存活
  void updateNodeAlive();
  void updateNodeName(const std::string& node_name);
  void removeNode();                               // 删除节点
  void addNode(const NodeInfo& node_info);         // 新增节点
  void updateNodeInfo(const NodeInfo& node_info);  // 更新指定id节点信息非新增
  void getNodeInfo(NodeInfo& node_info);           // 获取指定id节点信息
  int getNextNodeId();                             // 获取下一个空闲节点id
  int getAliveNodeCount();
  int getNodeCount();
  void printRegistry();

  //   NodesInfo *getNodesInfoPtr();
  //   TopicsInfo *getTopicsInforPtr();
  //   ShmManagerHead *getShmManagerHeaderPtr();
  // void readShmManagerHeaderInfo();

  // 事件通知相关方法（使用独立的事件通知共享内存）
  // 等待事件（带超时），返回当前的事件标志位
  std::bitset<EVENT_MAX_COUNT> waitForEvent(uint64_t timeout_ms) {
    return event_notification_shm_->waitForEvent(timeout_ms);
  }

  // 读取事件标志位（不清除）
  std::bitset<EVENT_MAX_COUNT> getTriggerEvent() {
    return event_notification_shm_->readEvents();
  }

  // 读取并清除事件标志位
  std::bitset<EVENT_MAX_COUNT> readAndClearEvents() {
    return event_notification_shm_->readAndClearEvents();
  }

  // 唤醒所有等待事件的线程（用于退出时）
  void notifyAllWaiters() { event_notification_shm_->notifyAll(); }

  // 事件触发相关方法
  // 注册 topic+event 组合，返回分配的 event_id（位索引）
  int registerTopicEvent(const std::string& topic_name,
                         const std::string& event_name);

  // 查找 topic+event 对应的 event_id，如果不存在返回 -1
  int getTopicEventId(const std::string& topic_name,
                      const std::string& event_name);

  // 触发事件：设置对应的位并通知条件变量
  void triggerEvent(const std::string& topic_name,
                    const std::string& event_name);

  // 清除事件标志位
  void clearTriggerEvent(int event_id);
  void clearAllTriggerEvents();

  // 注意：readAndClearEventFlags 已废弃，使用 readAndClearEvents() 代替
  // 如果需要清除特定的事件位，可以在读取后手动清除

  bool isTopicExist(const std::string& topic_name,
                    const std::string& event_name);

  void setNodeId(int node_id) { node_id_ = node_id; };

  void readTopicsInfo();

  void readTopicsInfoUnlocked();

  // 注册表锁管理（用于保护本地 topics_ 和 nodes_ 数据访问）
  void shmManagerLockRegistry() { registry_mutex_.lock(); }

  void shmManagerUnlockRegistry() { registry_mutex_.unlock(); }

  void syncRegistryFromShm();

 private:
  void initializeRegistry_();
  // 内部方法：查找或创建 topic+event 映射
  int findOrCreateTopicEvent_(const std::string& topic_name,
                              const std::string& event_name);

  void writeRegistryToShm_();  // 写入注册表到共享内存
  void writeNodesInfo_();
  void writeTopicsInfo_();

  void writeRegistryToShmUnlocked_();
  void writeNodesInfoUnlocked_();
  void writeTopicsInfoUnlocked_();

  void readNodesInfo_();
  void readTopicsInfo_();
  int getTopicEventId_(const std::string& topic_name,
                       const std::string& event_name);

  // 触发事件（通过 event_id）
  void triggerEventById_(int event_id);
  int node_id_ = -1;
  NodesInfo nodes_;
  TopicsInfo topics_;
  ShmManagerInfo shm_manager_info_;
  std::shared_ptr<ShmBase> shm_;
  std::shared_ptr<EventNotificationShm>
      event_notification_shm_;  // 独立的事件通知共享内存
  int shm_fd_ = -1;
  //   uint64_t *time_ptr_ = nullptr;
  //   char *data_ptr_;

  // 进程内锁：保护 nodes_ 和 topics_ 的内存访问（非共享内存）
  // 所有对共享内存的读写操作由 ShmBase 的锁保护
  std::mutex registry_mutex_;
};