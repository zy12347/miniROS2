#include "mini_ros2/communication/shm_manager.h"
ShmManager::ShmManager() {
  std::cout << "shm_manager" << std::endl;
  shm_ = std::make_shared<ShmBase>(SHM_MANAGER_NAME, MAX_SHM_MANGER_SIZE);

  // 检查共享内存是否已存在
  if (shm_->Exists()) {
    // 已存在，直接打开
    shm_->Open();
    initializeRegistry(); //只有事先已存在共享内存信息才尝试拷贝注册表信息
  } else {
    // 不存在，创建新的
    shm_->Create();
    shm_->Open();
  }
};
void ShmManager::readNodesInfo() {
  char node_data[NODE_INFO_SIZE];
  memset(node_data, 0,
         NODE_INFO_SIZE); // 初始化为0
  try {
    shm_->Read(node_data, NODE_INFO_SIZE, TOPIC_INFO_SIZE);
    // NodesInfo *nodes = static_cast<NodesInfo *>(node_data);
    std::string jsonStr(node_data);

    // 检查是否为空或无效JSON
    if (jsonStr.empty() ||
        jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
      // 空数据，初始化默认值
      nodes_.nodes_count = 0;
      nodes_.ref_count = 1; // 创建者
      nodes_.alive_node_count = 0;
      return;
    }
    JsonValue json = JsonValue::deserialize(jsonStr);
    nodes_.nodes_count = json["node_count"].asInt();
    nodes_.ref_count = json["ref_count"].asInt();
    nodes_.alive_node_count = json["alive_node_count"].asInt();

    for (int i = 0; i < nodes_.nodes_count; i++) {
      nodes_.nodes[i].node_id = json["nodes"][i]["node_id"].asInt();
      nodes_.nodes[i].pid = json["nodes"][i]["pid"].asInt();
      nodes_.nodes[i].pub_topic_count =
          json["nodes"][i]["pub_topic_count"].asInt();
      nodes_.nodes[i].sub_topic_count =
          json["nodes"][i]["sub_topic_count"].asInt();
      std::strcpy(nodes_.nodes[i].node_name,
                  json["nodes"][i]["node_name"].asString().c_str());
      nodes_.nodes[i].is_alive = json["nodes"][i]["is_alive"].asBool();
      for (int j = 0; j < nodes_.nodes[i].pub_topic_count; j++) {
        std::strcpy(nodes_.nodes[i].pub_topics[j],
                    json["nodes"][i]["pub_topics"][j].asString().c_str());
      }
      for (int j = 0; j < nodes_.nodes[i].sub_topic_count; j++) {
        std::strcpy(nodes_.nodes[i].sub_topics[j],
                    json["nodes"][i]["sub_topics"][j].asString().c_str());
      }
      nodes_.nodes[i].last_heartbeat =
          json["nodes"][i]["last_heartbeat"].asInt();
    }
  } catch (const std::exception &e) {
    // JSON解析失败，初始化默认值
    std::cout << "Failed to parse JSON from shared memory: " << e.what()
              << std::endl;
    nodes_.nodes_count = 0;
    nodes_.ref_count = 1; // 创建者
    nodes_.alive_node_count = 0;
  }
}
void ShmManager::readTopicsInfo() {
  char topic_data[TOPIC_INFO_SIZE];
  memset(topic_data, 0,
         TOPIC_INFO_SIZE); // 初始化为0

  try {
    shm_->Read(topic_data, TOPIC_INFO_SIZE);
    // TopicsInfo *topics = static_cast<TopicsInfo *>(topic_data);
    std::string jsonStr(topic_data);

    // 检查是否为空或无效JSON
    if (jsonStr.empty() ||
        jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
      // 空数据，初始化默认值
      topics_.topics_count = 0;
      return;
    }
    JsonValue json = JsonValue::deserialize(jsonStr);
    topics_.topics_count = json["topic_count"].asInt();
    topics_.event_flag_ = json["event_flag_"].asInt();
    for (int i = 0; i < topics_.topics_count; i++) {
      topics_.topics[i].event_id_ = json["topics"][i]["topic_id"].asInt();
      std::strcpy(topics_.topics[i].name_,
                  json["topics"][i]["name"].asString().c_str());
    }

  } catch (const std::exception &e) {
    // JSON解析失败，初始化默认值
    std::cout << "Failed to parse JSON from shared memory: " << e.what()
              << std::endl;
    topics_.topics_count = 0;
  }
}
void ShmManager::initializeRegistry() {
  //初始化注册表直接从共享内存读取数据
  readTopicsInfo();
  readNodesInfo();
}
void ShmManager::updateNodeHeartbeat(int node_id) {
  nodes_.nodes[node_id].last_heartbeat = time(nullptr);
  writeRegistryToShm_(node_id);
  // TBD心跳更新会频繁写入共享内存，后面优化为修改而非覆写
};

bool ShmManager::isNodeAlive(int node_id) {
  bool is_alive = nodes_.nodes[node_id].is_alive;
  return is_alive;
};

void ShmManager::addNode(const NodeInfo &node_info) {
  nodes_.nodes[node_info.node_id] = node_info;
  nodes_.nodes_count++;
  nodes_.alive_node_count++;
  writeRegistryToShm_(node_info.node_id);
}

void ShmManager::updateNodeInfo(int node_id, const NodeInfo &node_info) {
  nodes_.nodes[node_id] = node_info;
  writeRegistryToShm_(node_id);
};
void ShmManager::removeNode(int node_id) {
  //   nodes.erase(nodes.begin() + node_id);
  nodes_.nodes[node_id].is_alive = false;
  nodes_.alive_node_count--;
  nodes_.nodes_count--;
}

void ShmManager::getNodeInfo(int node_id, NodeInfo &node_info) {
  node_info = nodes_.nodes[node_id];
}
// TBD 这里的MAX_NODE_COUNT需要从配置文件中读取
int ShmManager::getNextNodeId() {
  for (int i = 0; i < MAX_NODE_COUNT; i++) {
    if (!nodes_.nodes[i].is_alive) {
      return i;
    }
  }
  return -1;
}

int ShmManager::getAliveNodeCount() {
  int count = nodes_.alive_node_count;
  return count;
}
int ShmManager::getNodeCount() {
  int count = nodes_.nodes_count;
  return count;
}

// void ShmManager::writeShmHead() {}
void ShmManager::writeNodesInfo() {
  JsonValue json;
  json["node_count"] = nodes_.nodes_count;
  json["ref_count"] = nodes_.ref_count;
  json["alive_node_count"] = nodes_.alive_node_count;
  json["nodes"] = JsonArray(nodes_.nodes_count);
  // TBD 先用覆写的方法后面用替换的方法
  for (int i = 0; i < nodes_.nodes_count; i++) {
    json["nodes"][i]["node_id"] = nodes_.nodes[i].node_id;
    json["nodes"][i]["pid"] = nodes_.nodes[i].pid;
    json["nodes"][i]["pub_topic_count"] = nodes_.nodes[i].pub_topic_count;
    json["nodes"][i]["sub_topic_count"] = nodes_.nodes[i].sub_topic_count;
    json["nodes"][i]["node_name"] = std::string(nodes_.nodes[i].node_name);
    json["nodes"][i]["is_alive"] = nodes_.nodes[i].is_alive;
    json["nodes"][i]["pub_topics"] = JsonArray(nodes_.nodes[i].pub_topic_count);
    json["nodes"][i]["sub_topics"] = JsonArray(nodes_.nodes[i].sub_topic_count);
    for (int j = 0; j < nodes_.nodes[i].pub_topic_count; j++) {
      json["nodes"][i]["pub_topics"][j] =
          std::string(nodes_.nodes[i].pub_topics[j]);
    }
    for (int j = 0; j < nodes_.nodes[i].sub_topic_count; j++) {
      json["nodes"][i]["sub_topics"][j] =
          std::string(nodes_.nodes[i].sub_topics[j]);
    }
    json["nodes"][i]["last_heartbeat"] = nodes_.nodes[i].last_heartbeat;
  }
  shm_->Write(json.serialize().c_str(), json.serialize().size(),
              TOPIC_INFO_SIZE);
}
void ShmManager::writeTopicsInfo() {
  JsonValue json;
  json["topic_count"] = topics_.topics_count;
  json["event_flag_"] = topics_.event_flag_;
  json["topics"] = JsonArray(topics_.topics_count);
  // TBD 先用覆写的方法后面用替换的方法
  for (int i = 0; i < topics_.topics_count; i++) {
    json["topics"][i]["topic_id"] = topics_.topics[i].event_id_;
    json["topics"][i]["name"] = topics_.topics[i].name_;
  }
  shm_->Write(json.serialize().c_str(), json.serialize().size());
}
void ShmManager::writeRegistryToShm_(int node_id) {
  // writeShmHead();
  writeTopicsInfo();
  writeNodesInfo();
}

void ShmManager::addSubTopic(int node_id, const std::string &topic_name) {
  if (nodes_.nodes[node_id].sub_topic_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(
        nodes_.nodes[node_id].sub_topics[nodes_.nodes[node_id].sub_topic_count],
        topic_name.c_str());
    nodes_.nodes[node_id].sub_topic_count++;
    writeRegistryToShm_(node_id);
  }
}

void ShmManager::addPubTopic(int node_id, const std::string &topic_name) {
  if (nodes_.nodes[node_id].pub_topic_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(
        nodes_.nodes[node_id].pub_topics[nodes_.nodes[node_id].pub_topic_count],
        topic_name.c_str());
    nodes_.nodes[node_id].pub_topic_count++;
    writeRegistryToShm_(node_id);
  }
}

void ShmManager::removeSubTopic(int node_id, const std::string &topic_name) {
  for (int i = 0; i < nodes_.nodes[node_id].sub_topic_count; i++) {
    if (nodes_.nodes[node_id].sub_topics[i] == topic_name) {
      // 移除topic，将后面的元素前移
      for (int j = i; j < nodes_.nodes[node_id].sub_topic_count - 1; j++) {
        std::strcpy(nodes_.nodes[node_id].sub_topics[j],
                    nodes_.nodes[node_id].sub_topics[j + 1]);
      }
      nodes_.nodes[node_id].sub_topic_count--;
      writeRegistryToShm_(node_id);
      break;
    }
  }
}

void ShmManager::removePubTopic(int node_id, const std::string &topic_name) {
  for (int i = 0; i < nodes_.nodes[node_id].pub_topic_count; i++) {
    if (nodes_.nodes[node_id].pub_topics[i] == topic_name) {
      // 移除topic，将后面的元素前移
      for (int j = i; j < nodes_.nodes[node_id].pub_topic_count - 1; j++) {
        std::strcpy(nodes_.nodes[node_id].pub_topics[j],
                    nodes_.nodes[node_id].pub_topics[j + 1]);
      }
      nodes_.nodes[node_id].pub_topic_count--;
      writeRegistryToShm_(node_id);
      break;
    }
  }
}

void ShmManager::updateNodeAlive(int node_id) {
  nodes_.nodes[node_id].is_alive = true;
  writeRegistryToShm_(node_id);
}
void ShmManager::updateNodeName(int node_id, const std::string &node_name) {
  std::strcpy(nodes_.nodes[node_id].node_name, node_name.c_str());
  writeRegistryToShm_(node_id);
}

void ShmManager::printRegistry() {
  // char data[MAX_SHM_MANGER_SIZE];
  // memset(data, 0, MAX_SHM_MANGER_SIZE); // 初始化为0
  // shm_->Read(data, MAX_SHM_MANGER_SIZE);
  // std::string jsonStr(data);
  // std::cout << jsonStr << std::endl;
  std::cout << "printRegistry" << std::endl;
}