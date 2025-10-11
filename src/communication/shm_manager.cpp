#include "mini_ros2/communication/shm_manager.h"

ShmManager::~ShmManager(){
    pthread_mutex_destroy(&mutex);
};

ShmManager::ShmManager(){
    shm_ = std::make_shared<ShmBase>(SHM_MANAGER_NAME, MAX_SHM_MANGER_SIZE);
    
    // 检查共享内存是否已存在
    if (shm_->Exists()) {
        // 已存在，直接打开
        shm_->Open();
        initializeRegistry();//只有事先已存在共享内存信息才尝试拷贝注册表信息
    } else {
        // 不存在，创建新的
        shm_->Create();
        shm_->Open();
    }
    // 初始化互斥锁
    pthread_mutex_init(&mutex, nullptr);
};

void ShmManager::initializeRegistry(){
    //初始化注册表直接从共享内存读取数据
    char data[MAX_SHM_MANGER_SIZE];
    memset(data, 0, MAX_SHM_MANGER_SIZE); // 初始化为0
    
    try {
        shm_->Read(data, MAX_SHM_MANGER_SIZE);
        std::string jsonStr(data);
        
        // 检查是否为空或无效JSON
        if (jsonStr.empty() || jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
            // 空数据，初始化默认值
            node_count = 0;
            ref_count = 1; // 创建者
            alive_node_count = 0;
            return;
        }
        
        JsonValue json = JsonValue::deserialize(jsonStr);
        node_count = json["node_count"].asInt();
        ref_count = json["ref_count"].asInt();
        alive_node_count = json["alive_node_count"].asInt();
        
        for(int i = 0; i < node_count; i++){
            nodes[i].node_id = json["nodes"][i]["node_id"].asInt();
            nodes[i].pid = json["nodes"][i]["pid"].asInt();
            nodes[i].pub_topic_count = json["nodes"][i]["pub_topic_count"].asInt();
            nodes[i].sub_topic_count = json["nodes"][i]["sub_topic_count"].asInt();
            nodes[i].node_name = json["nodes"][i]["node_name"].asString();
            nodes[i].is_alive = json["nodes"][i]["is_alive"].asBool();
            for(int j = 0; j < nodes[i].pub_topic_count; j++){
                nodes[i].pub_topics[j] = json["nodes"][i]["pub_topics"][j].asString();
            }
            for(int j = 0; j < nodes[i].sub_topic_count; j++){
                nodes[i].sub_topics[j] = json["nodes"][i]["sub_topics"][j].asString();
            }
            nodes[i].last_heartbeat = json["nodes"][i]["last_heartbeat"].asInt();
        }
    } catch (const std::exception& e) {
        // JSON解析失败，初始化默认值
        std::cout << "Failed to parse JSON from shared memory: " << e.what() << std::endl;
        node_count = 0;
        ref_count = 1; // 创建者
        alive_node_count = 0;
    }
}
void ShmManager::updateNodeHeartbeat(int node_id){
    //更新指定id节点心跳
    pthread_mutex_lock(&mutex);
    nodes[node_id].last_heartbeat = time(nullptr);
    writeRegistryToShm_(node_id);
    pthread_mutex_unlock(&mutex);
    //TBD心跳更新会频繁写入共享内存，后面优化为修改而非覆写
};

bool ShmManager::isNodeAlive(int node_id){
    pthread_mutex_lock(&mutex);
    bool is_alive = nodes[node_id].is_alive;
    pthread_mutex_unlock(&mutex);
    return is_alive;
};

void ShmManager::addNode(const NodeInfo &node_info){
    pthread_mutex_lock(&mutex);
    nodes[node_info.node_id] = node_info;
    node_count++;
    alive_node_count++;
    writeRegistryToShm_(node_info.node_id);
    pthread_mutex_unlock(&mutex);
}

void ShmManager::updateNodeInfo(int node_id, const NodeInfo &node_info){
    pthread_mutex_lock(&mutex);
    nodes[node_id] = node_info;
    writeRegistryToShm_(node_id);
    pthread_mutex_unlock(&mutex);
};
void ShmManager::removeNode(int node_id) {
    pthread_mutex_lock(&mutex);
    //   nodes.erase(nodes.begin() + node_id);
    alive_node_count--;
    node_count--;
    nodes[node_id].is_alive = false;
    pthread_mutex_unlock(&mutex);
}

void ShmManager::getNodeInfo(int node_id, NodeInfo &node_info) {
    pthread_mutex_lock(&mutex);
    node_info = nodes[node_id];
    pthread_mutex_unlock(&mutex);
}

int ShmManager::getNextNodeId() {
    for (int i = 0; i < MAX_NODE_COUNT; i++) {
        if (!nodes[i].is_alive) {
            return i;
        }
    }
    return -1;
}

int ShmManager::getAliveNodeCount(){
    pthread_mutex_lock(&mutex);
    int count = alive_node_count;
    pthread_mutex_unlock(&mutex);
    return count;
}
int ShmManager::getNodeCount(){
    pthread_mutex_lock(&mutex);
    int count = node_count;
    pthread_mutex_unlock(&mutex);
    return count;
}

void ShmManager::writeRegistryToShm_(int node_id) {
    JsonValue json;
    json["node_count"]       = node_count;
    json["ref_count"]        = ref_count;
    json["alive_node_count"] = alive_node_count;
    json["nodes"] = JsonArray(node_count);
    // TBD 先用覆写的方法后面用替换的方法
    for (int i = 0; i < node_count; i++) {
        json["nodes"][i]["node_id"]         = nodes[i].node_id;
        json["nodes"][i]["pid"]             = nodes[i].pid;
        json["nodes"][i]["pub_topic_count"] = nodes[i].pub_topic_count;
        json["nodes"][i]["sub_topic_count"] = nodes[i].sub_topic_count;
        json["nodes"][i]["node_name"]       = nodes[i].node_name;
        json["nodes"][i]["is_alive"]        = nodes[i].is_alive;
        json["nodes"][i]["pub_topics"] = JsonArray(nodes[i].pub_topic_count);
        json["nodes"][i]["sub_topics"] = JsonArray(nodes[i].sub_topic_count);
        for (int j = 0; j < nodes[i].pub_topic_count; j++) {
            json["nodes"][i]["pub_topics"][j] = nodes[i].pub_topics[j];
        }
        for (int j = 0; j < nodes[i].sub_topic_count; j++) {
            json["nodes"][i]["sub_topics"][j] = nodes[i].sub_topics[j];
        }
        json["nodes"][i]["last_heartbeat"] = nodes[i].last_heartbeat;
    }
    shm_->Write(json.serialize().c_str(), json.serialize().size());
}

void ShmManager::addSubTopic(int node_id, const std::string &topic_name) {
    pthread_mutex_lock(&mutex);
    if (nodes[node_id].sub_topic_count < MAX_TOPICS_PER_NODE) {
        nodes[node_id].sub_topics[nodes[node_id].sub_topic_count] = topic_name;
        nodes[node_id].sub_topic_count++;
        writeRegistryToShm_(node_id);
    }
    pthread_mutex_unlock(&mutex);
}

void ShmManager::addPubTopic(int node_id, const std::string &topic_name) {
    pthread_mutex_lock(&mutex);
    if (nodes[node_id].pub_topic_count < MAX_TOPICS_PER_NODE) {
        nodes[node_id].pub_topics[nodes[node_id].pub_topic_count] = topic_name;
        nodes[node_id].pub_topic_count++;
        writeRegistryToShm_(node_id);
    }
    pthread_mutex_unlock(&mutex);
}

void ShmManager::removeSubTopic(int node_id, const std::string &topic_name) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < nodes[node_id].sub_topic_count; i++) {
        if (nodes[node_id].sub_topics[i] == topic_name) {
            // 移除topic，将后面的元素前移
            for (int j = i; j < nodes[node_id].sub_topic_count - 1; j++) {
                nodes[node_id].sub_topics[j] = nodes[node_id].sub_topics[j + 1];
            }
            nodes[node_id].sub_topic_count--;
            writeRegistryToShm_(node_id);
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void ShmManager::removePubTopic(int node_id, const std::string &topic_name) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < nodes[node_id].pub_topic_count; i++) {
        if (nodes[node_id].pub_topics[i] == topic_name) {
            // 移除topic，将后面的元素前移
            for (int j = i; j < nodes[node_id].pub_topic_count - 1; j++) {
                nodes[node_id].pub_topics[j] = nodes[node_id].pub_topics[j + 1];
            }
            nodes[node_id].pub_topic_count--;
            writeRegistryToShm_(node_id);
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void ShmManager::updateNodeAlive(int node_id) {
    pthread_mutex_lock(&mutex);
    nodes[node_id].is_alive = true;
    writeRegistryToShm_(node_id);
    pthread_mutex_unlock(&mutex);
}

void ShmManager::updateNodeName(int node_id, const std::string &node_name) {
    pthread_mutex_lock(&mutex);
    nodes[node_id].node_name = node_name;
    writeRegistryToShm_(node_id);
    pthread_mutex_unlock(&mutex);
}

void ShmManager::printRegistry() {
    pthread_mutex_lock(&mutex);
    char data[MAX_SHM_MANGER_SIZE];
    memset(data, 0, MAX_SHM_MANGER_SIZE); // 初始化为0
    shm_->Read(data, MAX_SHM_MANGER_SIZE);
    std::string jsonStr(data);
    std::cout << jsonStr << std::endl;
    pthread_mutex_unlock(&mutex);
}