#pragma once
#include <pthread.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "mini_ros2/communication/event_notification_shm.h"
#include "mini_ros2/communication/shared_memory.h"

#define MAX_TOPICS_PER_NODE EVENT_MAX_COUNT
#define MAX_NODE_COUNT 16
#define MAX_NODE_NAME_LEN 64
// MAX_TOPIC_NAME_LEN 从 64 减少到 32，可节省约 32 KB（如果使用1024 topics）
// 但配合 256 topics 时，节省约 8 KB
#define MAX_TOPIC_NAME_LEN 32  // 原为 64
#define SHM_MANAGER_NAME "/miniros2_dds_shm_manager"
#define SHM_MANAGER_SIZE sizeof(ShmManagerData)
//sizeof(uint32_t) + sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) + sizeof(uint64_t) + sizeof(TopicsInfo) + sizeof(NodesInfo)


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
  int sync_topic_count;
  bool is_alive;
  int last_heartbeat;
  char node_name[MAX_NODE_NAME_LEN];
};

struct NodesInfo {
  int nodes_count;
  int alive_node_count;
  NodeInfo nodes[MAX_NODE_COUNT];
};

// struct ShmManagerInfo {
//   TopicsInfo topic_info;
//   NodesInfo nodes_info;
// };

// 共享内存数据结构（类似 EventNotificationData）
struct ShmManagerData {
  uint32_t initialized_;  // 初始化标志：0x4D525332 = "MRS2" (MiniROS2)
  pthread_mutex_t mutex_;  // 互斥锁（进程间共享）
  pthread_cond_t cond_;    // 条件变量（进程间共享）
  uint64_t time_;           // 时间戳
  int32_t ref_count_;       // 引用计数（进程间共享）
  TopicsInfo topics_info_;  // Topics 信息
  NodesInfo nodes_info_;    // Nodes 信息
  // char padding_[SHM_MANAGER_SIZE - sizeof(uint32_t) - sizeof(pthread_mutex_t) -
  //               sizeof(pthread_cond_t) - sizeof(uint64_t) - sizeof(int32_t) -
  //               sizeof(TopicsInfo) - sizeof(NodesInfo)];  // 填充到固定大小
};

class ShmManager {
 public:
  // 单例模式：获取唯一实例指针（双重检查锁定优化）
  static ShmManager* Instance() {
    // 第一次检查：避免每次调用都加锁（读操作，无锁）
    ShmManager* tmp = instance_;
    if (tmp == nullptr) {
      // 第二次检查：在锁内再次检查，确保只创建一次
      std::lock_guard<std::mutex> lock(creat_mutex_);
      tmp = instance_;
      if (tmp == nullptr) {
        instance_ = new ShmManager();
        // 注册退出时清理函数
        std::atexit(Cleanup);
        tmp = instance_;
      }
    }
    return tmp;
  }

  // 清理单例实例（在程序退出时调用）
  static void Cleanup() {
    creat_mutex_.lock();
    if (instance_ != nullptr) {
      delete instance_;
      instance_ = nullptr;
    }
    creat_mutex_.unlock();
  }

  // 去中心化构造函数：检查共享内存是否存在，不存在则创建
  ShmManager();

  // 析构函数：清理共享内存
  ~ShmManager();
  void addSubTopic(const std::string& topic_name,
                   const std::string& event_name);
  void addPubTopic(const std::string& topic_name,
                   const std::string& event_name);
  void addSyncTopic(const std::string& topic_name,
                   const std::string& event_name);
  // void removeSubTopic(const std::string& topic_name,
  //                     const std::string& event_name);
  // void removePubTopic(const std::string& topic_name,
  //                     const std::string& event_name);
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

  std::bitset<EVENT_MAX_COUNT> waitForEventResponse(uint64_t timeout_ms) {
    return event_notification_shm_->waitForEventResponse(timeout_ms);
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

  void triggerEventResponse(const std::string& topic_name,
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

  // 打开已存在的共享内存
  void Open();

 private:
  void initializeRegistry_();
  // 内部方法：查找或创建 topic+event 映射
  int findOrCreateTopicEvent_(const std::string& topic_name,
                              const std::string& event_name);

  int findOrCreateTopicEventUnlocked_(const std::string& topic_name,
                                      const std::string& event_name);

  // void writeRegistryToShm_();  // 写入注册表到共享内存
  // void writeNodesInfo_();
  // void writeTopicsInfo_();

  // void writeRegistryToShmUnlocked_();
  // void writeNodesInfoUnlocked_();
  // void writeTopicsInfoUnlocked_();

  void readNodesInfo_();
  void readTopicsInfo_();
  int getTopicEventId_(const std::string& topic_name,
                       const std::string& event_name);

  // 触发事件（通过 event_id）
  void triggerEventById_(int event_id);

  // 初始化互斥锁和条件变量
  void initMutexAndCond();

  // 缓存指针
  void cachePointers();

  int node_id_ = -1;
  std::shared_ptr<SharedMemory> shm_;
  std::shared_ptr<EventNotificationShm>
      event_notification_shm_;  // 独立的事件通知共享内存
  bool is_owner_ = false;

  // 缓存的共享内存指针
  ShmManagerData* data_ptr_ = nullptr;
  pthread_mutex_t* mutex_ptr_ = nullptr;
  pthread_cond_t* cond_ptr_ = nullptr;
  uint64_t* time_ptr_ = nullptr;
  int32_t* ref_count_ptr_ = nullptr;  // 引用计数指针
  TopicsInfo* topics_info_ptr_ = nullptr;
  NodesInfo* nodes_info_ptr_ = nullptr;

  // 引用计数管理
  void incrementRefCount();
  void decrementRefCount();

  // 进程内锁：保护本地 nodes_ 和 topics_ 的内存访问（非共享内存）
  // 所有对共享内存的读写操作由共享内存的互斥锁保护
  std::mutex registry_mutex_;

  // 单例模式相关
  static ShmManager* instance_;
  static std::mutex creat_mutex_;
};

// 宏定义：使用 SHM_MANAGER 替代 Instance()
#define SHM_MANAGER ShmManager::Instance()