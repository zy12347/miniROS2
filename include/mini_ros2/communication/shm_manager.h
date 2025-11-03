#include <pthread.h>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#define MAX_NODE_COUNT 16
#define MAX_TOPICS_PER_NODE 32
#define MAX_NODE_NAME_LEN 256
#define MAX_TOPIC_NAME_LEN 256
#define MAX_SHM_MANGER_SIZE 4 * 1024 * 1024
#define TOPIC_INFO_SIZE 1024 * 1024
#define NODE_INFO_SIZE 3 * 1024 * 1024
#define SHM_MANAGER_NAME "/miniros2_dds_shm_manager20"

struct TopicInfo {
  char name_[MAX_TOPIC_NAME_LEN];
  int event_id_;
  // TopicInfo() {
  //   name_[0] = "\0";
  //   event_id_ = -1;
  // }
};

struct TopicsInfo {
  int topics_count;
  int event_flag_; // 0为默认registry变动,第i位表示第i个事件变动
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
  // std::string node_namespace;
  char pub_topics[MAX_TOPICS_PER_NODE][MAX_TOPIC_NAME_LEN];
  char sub_topics[MAX_TOPICS_PER_NODE][MAX_TOPIC_NAME_LEN];
  // NodeInfo() {
  //   node_id = -1;
  //   pid = -1;
  //   pub_topic_count = -1;
  //   sub_topic_count = -1;
  //   node_name[0] = '\0'; // 确保字符串以null结尾
  //   is_alive = false;
  //   last_heartbeat = -1;
  //   // 初始化所有topic数组元素为空字符串
  //   for (int i = 0; i < MAX_TOPICS_PER_NODE; i++) {
  //     pub_topics[i][0] = '\0';
  //     sub_topics[i][0] = '\0';
  //   }
  // }
};

struct NodesInfo {
  int nodes_count;
  int alive_node_count;
  int ref_count;
  NodeInfo nodes[MAX_NODE_COUNT];
};

class ShmManager {
public:
  // 去中心化构造函数：检查共享内存是否存在，不存在则创建
  ShmManager();

  //   ~ShmManager();
  void addSubTopic(const std::string &topic_name, const std::string &event_name);
  void addPubTopic(const std::string &topic_name, const std::string &event_name);
  void removeSubTopic(const std::string &topic_name, const std::string &event_name);
  void removePubTopic(const std::string &topic_name, const std::string &event_name);
  void updateNodeHeartbeat(); //更新指定id节点心跳
  bool isNodeAlive();         //判断指定id节点是否存活
  void updateNodeAlive();
  void updateNodeName(const std::string &node_name);
  void removeNode();            //删除节点
  void addNode(const NodeInfo &node_info); //新增节点
  void updateNodeInfo(const NodeInfo &node_info); //更新指定id节点信息非新增
  void getNodeInfo(NodeInfo &node_info); //获取指定id节点信息
  int getNextNodeId(); //获取下一个空闲节点id
  int getAliveNodeCount();
  int getNodeCount();
  void printRegistry();

  //   NodesInfo *getNodesInfoPtr();
  //   TopicsInfo *getTopicsInforPtr();
  //   ShmManagerHead *getShmManagerHeaderPtr();
  // void readShmManagerHeaderInfo();

  void shmManagerWait() { 
    std::lock_guard<std::mutex> lock(registry_mutex_);
    shm_->shmBaseWait(); 
};

  // 注意：shmManagerLock/Unlock 用于条件变量等待（使用 ShmBase 的锁）
  void shmManagerLock() { 
    std::lock_guard<std::mutex> lock(registry_mutex_);
    shm_->shmBaseLock(); 
};

  void shmManagerUnlock() { 
    std::lock_guard<std::mutex> lock(registry_mutex_);
    shm_->shmBaseUnlock(); 
};

  void shmManagerSignal() { 
    std::lock_guard<std::mutex> lock(registry_mutex_);
    shm_->shmBaseSignal(); 
};

  int getTriggerEvent() { 
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return topics_.event_flag_; 
  };

  // 事件触发相关方法
  // 注册 topic+event 组合，返回分配的 event_id（位索引）
  int registerTopicEvent(const std::string &topic_name, const std::string &event_name);
  
  // 查找 topic+event 对应的 event_id，如果不存在返回 -1
  int getTopicEventId(const std::string &topic_name, const std::string &event_name);
  
  // 触发事件：设置对应的位并通知条件变量
  void triggerEvent(const std::string &topic_name, const std::string &event_name);
  
  // 清除事件标志位
  void clearTriggerEvent(int event_id);
  void clearAllTriggerEvents();
  
  // 批量读取并清除事件标志位（原子操作）
  // 返回当前的事件标志位，并清除指定的位
  int readAndClearEventFlags(const std::vector<int> &event_ids_to_clear);

  bool isTopicExist(const std::string &topic_name, const std::string &event_name);

  void setNodeId(int node_id) { node_id_ = node_id; };

private:
void initializeRegistry_();
  // 内部方法：查找或创建 topic+event 映射
  int findOrCreateTopicEvent_(const std::string &topic_name, const std::string &event_name);

  void writeRegistryToShm_(); //写入注册表到共享内存
  void writeNodesInfo_();
  void writeTopicsInfo_();

  void readNodesInfo_();
  void readTopicsInfo_();
  int getTopicEventId_(const std::string &topic_name, const std::string &event_name);

  void shmManagerBroadcast_() { shm_->shmBaseBroadcast(); };
  // 触发事件（通过 event_id）
  void triggerEventById_(int event_id);
  int node_id_ = -1;
  NodesInfo nodes_;
  TopicsInfo topics_;
  std::shared_ptr<ShmBase> shm_;
  int shm_fd_ = -1;
  //   uint64_t *time_ptr_ = nullptr;
  //   char *data_ptr_;
  
  // 进程内锁：保护 nodes_ 和 topics_ 的内存访问（非共享内存）
  // 所有对共享内存的读写操作由 ShmBase 的锁保护
  std::mutex registry_mutex_;
};