#include <pthread.h>

#include "mini_ros2/communication/shm_base.h"
#include "mini_ros2/message/json.h"
#include <atomic>
#include <iostream>
#include <string>
#include <vector>

#define MAX_NODE_COUNT 256
#define MAX_TOPICS_PER_NODE 64
#define MAX_NODE_NAME_LEN 256
#define MAX_TOPIC_NAME_LEN 256
#define MAX_SHM_MANGER_SIZE 1024 * 1024
#define SHM_MANAGER_HEAD_SIZE 1024
#define TOPIC_INFO_SIZE 23 * 1024
#define NODE_INFO_SIZE 1000 * 1024
#define SHM_MANAGER_NAME "/miniros2_dds_shm_manager"

struct TopicInfo {
  char name_[MAX_TOPIC_NAME_LEN];
  uint64_t event_id_;
  TopicInfo() {
    name_[0] = "\0";
    event_id_ = -1;
  }
};

struct TopicsInfo {
  int topic_count;
  TopicInfo topics[0];
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
  NodeInfo() {
    node_id = -1;
    pid = -1;
    pub_topic_count = -1;
    sub_topic_count = -1;
    node_name[0] = '\0'; // 确保字符串以null结尾
    is_alive = false;
    last_heartbeat = -1;
    // 初始化所有topic数组元素为空字符串
    for (int i = 0; i < MAX_TOPICS_PER_NODE; i++) {
      pub_topics[i][0] = '\0';
      sub_topics[i][0] = '\0';
    }
  }
};

struct NodesInfo {
  int nodes_count;
  int alive_node_count;
  int ref_count;
  NodeInfo nodes[0];
};

class ShmManager {
public:
  // 去中心化构造函数：检查共享内存是否存在，不存在则创建
  ShmManager();

  //   ~ShmManager();

  void initializeRegistry();
  void addSubTopic(int node_id, const std::string &topic_name);
  void addPubTopic(int node_id, const std::string &topic_name);
  void removeSubTopic(int node_id, const std::string &topic_name);
  void removePubTopic(int node_id, const std::string &topic_name);
  void updateNodeHeartbeat(int node_id); //更新指定id节点心跳
  bool isNodeAlive(int node_id);         //判断指定id节点是否存活
  void updateNodeAlive(int node_id);
  void updateNodeName(int node_id, const std::string &node_name);
  void removeNode(int node_id);            //删除节点
  void addNode(const NodeInfo &node_info); //新增节点
  void updateNodeInfo(int node_id,
                      const NodeInfo &node_info); //更新指定id节点信息非新增
  void writeRegistryToShm_(int node_id); //写入注册表到共享内存
  void getNodeInfo(int node_id, NodeInfo &node_info); //获取指定id节点信息
  int getNextNodeId(); //获取下一个空闲节点id
  int getAliveNodeCount();
  int getNodeCount();
  void printRegistry();

  //   NodesInfo *getNodesInfoPtr();
  //   TopicsInfo *getTopicsInforPtr();
  //   ShmManagerHead *getShmManagerHeaderPtr();
  void readNodesInfo();
  void readTopicsInfo();
  void readShmManagerHeaderInfo();

private:
  NodesInfo nodes_;
  TopicsInfo topics_;
  std::shared_ptr<ShmBase> shm_;
  int shm_fd_ = -1;
  //   pthread_mutex_t *mutex_ptr_ = nullptr;
  //   pthread_cond_t *cond_ptr_ = nullptr;
  //   uint64_t *time_ptr_ = nullptr;
  //   char *data_ptr_;
};