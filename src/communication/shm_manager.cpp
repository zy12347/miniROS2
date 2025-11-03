#include "mini_ros2/communication/shm_manager.h"
ShmManager::ShmManager() {
  std::cout << "shm_manager" << std::endl;
  shm_ = std::make_shared<ShmBase>(SHM_MANAGER_NAME, MAX_SHM_MANGER_SIZE);

  // 检查共享内存是否已存在
  if (shm_->Exists()) {
    // 已存在，直接打开
    shm_->Open();
    std::cout << "shm_manager open" << std::endl;
    initializeRegistry(); //只有事先已存在共享内存信息才尝试拷贝注册表信息
  } else {
    // 不存在，创建新的
    std::cout << "shm_manager create" << std::endl;
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
  std::cout << "readTopicsInfo" << std::endl;
  try {
    std::cout << "shm_->Read" << std::endl;
    shm_->Read(topic_data, TOPIC_INFO_SIZE);
    std::cout << "shm_->Read done" << std::endl;
    // TopicsInfo *topics = static_cast<TopicsInfo *>(topic_data);
    std::string jsonStr(topic_data);
    std::cout << "jsonStr: " << jsonStr << std::endl;
    // 检查是否为空或无效JSON
    if (jsonStr.empty() ||
        jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
      // 空数据，初始化默认值
      std::cout << "empty" << std::endl;
      topics_.topics_count = 0;
      return;
    }
    std::cout << "not empty" << std::endl;
    JsonValue json = JsonValue::deserialize(jsonStr);
    std::cout << "json: " << json.serialize() << std::endl;
    topics_.topics_count = json["topic_count"].asInt();
    topics_.event_flag_ = json["event_flag_"].asInt();
    for (int i = 0; i < topics_.topics_count; i++) {
      topics_.topics[i].event_id_ = json["topics"][i]["topic_id"].asInt();
      std::strcpy(topics_.topics[i].name_,
                  json["topics"][i]["name"].asString().c_str());
    }
    std::cout << "topics_count: " << topics_.topics_count << std::endl;
    std::cout << "event_flag_: " << topics_.event_flag_ << std::endl;
    for (int i = 0; i < topics_.topics_count; i++) {
      std::cout << "topic_id: " << topics_.topics[i].event_id_ << std::endl;
      std::cout << "name: " << topics_.topics[i].name_ << std::endl;
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
  std::cout << "initializeRegistry" << std::endl;
  readTopicsInfo();
  readNodesInfo();
}
void ShmManager::updateNodeHeartbeat() {
  nodes_.nodes[node_id_].last_heartbeat = time(nullptr);
  writeRegistryToShm_();
  // TBD心跳更新会频繁写入共享内存，后面优化为修改而非覆写
};

bool ShmManager::isNodeAlive() {
  bool is_alive = nodes_.nodes[node_id_].is_alive;
  return is_alive;
};

void ShmManager::addNode(const NodeInfo &node_info) {
  nodes_.nodes[node_id_] = node_info;
  nodes_.nodes_count++;
  nodes_.alive_node_count++;
  writeRegistryToShm_();
}

void ShmManager::updateNodeInfo(const NodeInfo &node_info) {
  nodes_.nodes[node_id_] = node_info;
  writeRegistryToShm_();
};
void ShmManager::removeNode() {
  //   nodes.erase(nodes.begin() + node_id);
  nodes_.nodes[node_id_].is_alive = false;
  nodes_.alive_node_count--;
  nodes_.nodes_count--;
}

void ShmManager::getNodeInfo(NodeInfo &node_info) {
  node_info = nodes_.nodes[node_id_];
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
// 内部写入方法：假设调用者已经持有锁（或不持有锁，根据情况）
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
// 内部写入方法：假设调用者已经持有锁（或不持有锁，根据情况）
void ShmManager::writeTopicsInfo() {
  JsonValue json;
  json["topic_count"] = topics_.topics_count;
  json["event_flag_"] = topics_.event_flag_;
  json["topics"] = JsonArray(topics_.topics_count);
  // TBD 先用覆写的方法后面用替换的方法
  for (int i = 0; i < topics_.topics_count; i++) {
    json["topics"][i]["topic_id"] = topics_.topics[i].event_id_;
    json["topics"][i]["name"] = std::string(topics_.topics[i].name_);
  }
  shm_->Write(json.serialize().c_str(), json.serialize().size());
}
void ShmManager::writeRegistryToShm_() {
  // writeShmHead();
  shmManagerLock(); // 添加锁保护
  writeTopicsInfo();
  writeNodesInfo();
  shmManagerUnlock();
}

void ShmManager::addSubTopic(const std::string &topic_name, const std::string &event_name) {
  shmManagerLock(); // 添加锁保护
  if (nodes_.nodes[node_id_].sub_topic_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(
        nodes_.nodes[node_id_].sub_topics[nodes_.nodes[node_id_].sub_topic_count],
        topic_name.c_str());
    nodes_.nodes[node_id_].sub_topic_count++;
    writeNodesInfo(); // 内部已持有锁，直接调用
  }
  if(topics_.topics_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(topics_.topics[topics_.topics_count].name_, event_name.c_str());
    int event_id = findOrCreateTopicEvent_(topic_name, event_name);
    if(event_id >= 0) {
      topics_.topics[event_id].event_id_ = event_id;
      topics_.topics_count++;
      writeTopicsInfo(); // 内部已持有锁，直接调用
    } else {
      std::cerr << "Failed to create topic event" << std::endl;
    }
    // 移除重复的 writeTopicsInfo() 调用
  }
  shmManagerUnlock();
}

void ShmManager::addPubTopic(const std::string &topic_name, const std::string &event_name) {
  shmManagerLock(); // 添加锁保护
  if (nodes_.nodes[node_id_].pub_topic_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(
        nodes_.nodes[node_id_].pub_topics[nodes_.nodes[node_id_].pub_topic_count],
        topic_name.c_str());
    nodes_.nodes[node_id_].pub_topic_count++;
    writeNodesInfo(); // 内部已持有锁，直接调用
  }
  if(topics_.topics_count < MAX_TOPICS_PER_NODE) {
    int event_id = findOrCreateTopicEvent_(topic_name, event_name);
    if(event_id >= 0) {
      topics_.topics[event_id].event_id_ = event_id;
      topics_.topics_count++;
      writeTopicsInfo(); // 内部已持有锁，直接调用
    } else {
      std::cerr << "Failed to create topic event" << std::endl;
    }
  }
  shmManagerUnlock();
}

void ShmManager::removeSubTopic(const std::string &topic_name, const std::string &event_name) {
  for (int i = 0; i < nodes_.nodes[node_id_].sub_topic_count; i++) {
    if (nodes_.nodes[node_id_].sub_topics[i] == topic_name) {
      // 移除topic，将后面的元素前移
      for (int j = i; j < nodes_.nodes[node_id_].sub_topic_count - 1; j++) {
        std::strcpy(nodes_.nodes[node_id_].sub_topics[j],
                    nodes_.nodes[node_id_].sub_topics[j + 1]);
      }
      nodes_.nodes[node_id_].sub_topic_count--;
      writeRegistryToShm_();
      break;
    }
  }
}

void ShmManager::removePubTopic(const std::string &topic_name, const std::string &event_name) {
  for (int i = 0; i < nodes_.nodes[node_id_].pub_topic_count; i++) {
    if (nodes_.nodes[node_id_].pub_topics[i] == topic_name) {
      // 移除topic，将后面的元素前移
      for (int j = i; j < nodes_.nodes[node_id_].pub_topic_count - 1; j++) {
        std::strcpy(nodes_.nodes[node_id_].pub_topics[j],
                    nodes_.nodes[node_id_].pub_topics[j + 1]);
      }
      nodes_.nodes[node_id_].pub_topic_count--;
      writeRegistryToShm_();
      break;
    }
  }
}

void ShmManager::updateNodeAlive() {
  nodes_.nodes[node_id_].is_alive = true;
  writeRegistryToShm_();
}
void ShmManager::updateNodeName(const std::string &node_name) {
  std::strcpy(nodes_.nodes[node_id_].node_name, node_name.c_str());
  writeRegistryToShm_();
}

void ShmManager::printRegistry() {
  // char data[MAX_SHM_MANGER_SIZE];
  // memset(data, 0, MAX_SHM_MANGER_SIZE); // 初始化为0
  // shm_->Read(data, MAX_SHM_MANGER_SIZE);
  // std::string jsonStr(data);
  // std::cout << jsonStr << std::endl;
  char topic_data[TOPIC_INFO_SIZE];
  memset(topic_data, 0, TOPIC_INFO_SIZE);
  shm_->Read(topic_data, TOPIC_INFO_SIZE);
  std::string jsonStr(topic_data);
  std::cout << "jsonStr: " << jsonStr << std::endl;
  JsonValue json = JsonValue::deserialize(jsonStr);
  std::cout << "topics_count: " << json["topic_count"].asInt() << std::endl;
  std::cout << "event_flag_: " << json["event_flag_"].asInt() << std::endl;
  for (int i = 0; i < json["topic_count"].asInt(); i++) {
    std::cout << "topic_id: " << json["topics"][i]["topic_id"].asInt() << std::endl;
    std::cout << "name: " << json["topics"][i]["name"].asString() << std::endl;
  }
  std::cout << "printRegistry" << std::endl;
}

// 查找或创建 topic+event 映射，返回 event_id（位索引）
int ShmManager::findOrCreateTopicEvent_(const std::string &topic_name, const std::string &event_name) {
  std::string full_name = topic_name + "_" + event_name;
  
  // 查找是否已存在
  for (int i = 0; i < topics_.topics_count; i++) {
    if (std::string(topics_.topics[i].name_) == full_name) {
      return topics_.topics[i].event_id_;
    }
  }
  
  // 不存在，创建新的映射
  if (topics_.topics_count >= MAX_TOPICS_PER_NODE) {
    std::cerr << "Maximum topic count reached" << std::endl;
    return -1;
  }
  
  // 分配新的 event_id（位索引）
  int new_event_id = topics_.topics_count;
  topics_.topics[new_event_id].event_id_ = new_event_id;
  std::strcpy(topics_.topics[new_event_id].name_, full_name.c_str());
  topics_.topics_count++;
  
  return new_event_id;
}

// 注册 topic+event 组合，返回分配的 event_id（位索引）
int ShmManager::registerTopicEvent(const std::string &topic_name, const std::string &event_name) {
  shmManagerLock();
  int event_id = findOrCreateTopicEvent_(topic_name, event_name);
  if (event_id >= 0) {
    writeTopicsInfo(); // 更新到共享内存
  }
  shmManagerUnlock();
  return event_id;
}

// 查找 topic+event 对应的 event_id，如果不存在返回 -1
int ShmManager::getTopicEventId(const std::string &topic_name, const std::string &event_name) {
  std::string full_name = topic_name + "_" + event_name;
  
  shmManagerLock();
  for (int i = 0; i < topics_.topics_count; i++) {
    if (std::string(topics_.topics[i].name_) == full_name) {
      int event_id = topics_.topics[i].event_id_;
      shmManagerUnlock();
      return event_id;
    }
  }
  shmManagerUnlock();
  return -1;
}

// 触发事件：设置对应的位并通知条件变量
void ShmManager::triggerEvent(const std::string &topic_name, const std::string &event_name) {
  std::cout << "triggerEvent: " << topic_name << " " << event_name << std::endl;
  int event_id = getTopicEventId(topic_name, event_name);
  if (event_id >= 0) {
    std::cout << "triggerEventById: " << event_id << std::endl;
    triggerEventById(event_id);
  }
}

// 触发事件（通过 event_id）
void ShmManager::triggerEventById(int event_id) {
  if (event_id < 0 || event_id >= MAX_TOPICS_PER_NODE) {
    return;
  }
  
  shmManagerLock();
  // 设置对应的位
  topics_.event_flag_ |= (1 << event_id);
  writeTopicsInfo(); // 更新到共享内存
  // 在持有锁的情况下通知（避免丢失通知）
  shmManagerBroadcast(); // 通知所有等待的线程
  shmManagerUnlock();
}

// 清除事件标志位
void ShmManager::clearTriggerEvent(int event_id) {
  if (event_id < 0 || event_id >= MAX_TOPICS_PER_NODE) {
    return;
  }
  
  shmManagerLock();
  topics_.event_flag_ &= ~(1 << event_id);
  writeTopicsInfo();
  shmManagerUnlock();
}

// 清除所有事件标志位
void ShmManager::clearAllTriggerEvents() {
  shmManagerLock();
  topics_.event_flag_ = 0;
  writeTopicsInfo();
  shmManagerUnlock();
}

// 批量读取并清除事件标志位（原子操作）
// 注意：调用者必须已经持有 ShmManager 锁
int ShmManager::readAndClearEventFlags(const std::vector<int> &event_ids_to_clear) {
  // 注意：此方法假设调用者已经持有锁，不再重复加锁
  int current_flags = topics_.event_flag_;
  
  // 清除指定的位
  for (int event_id : event_ids_to_clear) {
    if (event_id >= 0 && event_id < MAX_TOPICS_PER_NODE) {
      topics_.event_flag_ &= ~(1 << event_id);
    }
  }
  
  // 如果有清除操作，更新到共享内存
  if (!event_ids_to_clear.empty()) {
    writeTopicsInfo(); // writeTopicsInfo 不会加锁，只写数据
  }
  
  return current_flags; // 返回清除前的标志位
}

bool ShmManager::isTopicExist(const std::string &topic_name, const std::string &event_name) {
  std::string full_name = topic_name + "_" + event_name;
  for (int i = 0; i < topics_.topics_count; i++) {
    if (std::string(topics_.topics[i].name_) == full_name) {
      return true;
    }
  }
  return false;
}