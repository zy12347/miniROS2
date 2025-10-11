#include <pthread.h>

#include <atomic>
#include <iostream>
#include <string>
#include <vector>
#include "mini_ros2/message/json.h"
#include "mini_ros2/communication/shm_base.h"

#define MAX_NODE_COUNT      100
#define MAX_TOPICS_PER_NODE 100
#define MAX_NODE_NAME_LEN   100
#define MAX_TOPIC_NAME_LEN  100
#define MAX_SHM_MANGER_SIZE 1024*1024
#define SHM_MANAGER_NAME "/miniros2_dds_shm_manager"

struct NodeInfo {
    int node_id;
    int pid;
    int pub_topic_count;
    int sub_topic_count;
    std::string node_name;
    bool is_alive;
    // std::string node_namespace;
    std::vector<std::string> pub_topics;
    std::vector<std::string> sub_topics;
    int last_heartbeat;
    
    // 构造函数初始化vector大小
    NodeInfo() : pub_topics(MAX_TOPICS_PER_NODE), sub_topics(MAX_TOPICS_PER_NODE) {
        node_id = 0;
        pid = 0;
        pub_topic_count = 0;
        sub_topic_count = 0;
        is_alive = false;
        last_heartbeat = 0;
    }
};

class ShmManager {
 public:
    // 去中心化构造函数：检查共享内存是否存在，不存在则创建
    ShmManager();
    
    ~ShmManager();

    void initializeRegistry();
    void addSubTopic(int node_id, const std::string &topic_name);
    void addPubTopic(int node_id, const std::string &topic_name);
    void removeSubTopic(int node_id, const std::string &topic_name);
    void removePubTopic(int node_id, const std::string &topic_name);
    void updateNodeHeartbeat(int node_id);//更新指定id节点心跳
    bool isNodeAlive(int node_id);//判断指定id节点是否存活
    void updateNodeAlive(int node_id);
    void updateNodeName(int node_id, const std::string &node_name);
    void removeNode(int node_id);//删除节点
    void addNode(const NodeInfo &node_info);//新增节点
    void updateNodeInfo(int node_id, const NodeInfo &node_info);//更新指定id节点信息非新增
    void writeRegistryToShm_(int node_id);//写入注册表到共享内存
    void getNodeInfo(int node_id, NodeInfo &node_info);//获取指定id节点信息
    int getNextNodeId();//获取下一个空闲节点id
    int getAliveNodeCount();
    int getNodeCount();
    void printRegistry();
 private:
    int alive_node_count;
    int node_count;
    int ref_count;
    NodeInfo nodes[MAX_NODE_COUNT];
    pthread_mutex_t mutex;
    std::shared_ptr<ShmBase> shm_;
    int shm_fd_           = -1;
};