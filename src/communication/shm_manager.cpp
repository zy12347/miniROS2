#include "mini_ros2/communication/shm_manager.h"
ShmManager::ShmManager() {
  std::cout << "shm_manager" << std::endl;
  shm_ = std::make_shared<ShmBase>(SHM_MANAGER_NAME, MAX_SHM_MANGER_SIZE);

  // 初始化事件通知共享内存
  event_notification_shm_ = std::make_shared<EventNotificationShm>();
  if (event_notification_shm_->Exists()) {
    event_notification_shm_->Open();
    std::cout << "event_notification_shm open" << std::endl;
  } else {
    event_notification_shm_->Create();
    std::cout << "event_notification_shm create" << std::endl;
  }

  // 检查共享内存是否已存在
  if (shm_->Exists()) {
    // 已存在，直接打开
    shm_->Open();
    std::cout << "shm_manager open" << std::endl;
    initializeRegistry_();  // 只有事先已存在共享内存信息才尝试拷贝注册表信息
  } else {
    // 不存在，创建新的
    std::cout << "shm_manager create" << std::endl;
    shm_->Create();
    shm_->Open();
  }
};

ShmManager::~ShmManager() {
  std::cout << "ShmManager destructor: cleaning up shared memory" << std::endl;

  // 清理事件通知共享内存
  if (event_notification_shm_) {
    // EventNotificationShm 的析构函数会自动清理
    event_notification_shm_.reset();
  }

  // 清理注册表共享内存
  if (shm_) {
    // ShmBase 内部使用 SharedMemory，其析构函数会自动清理
    shm_.reset();
  }
}

void ShmManager::readNodesInfo_() {
  char node_data[NODE_INFO_SIZE];
  memset(node_data, 0,
         NODE_INFO_SIZE);  // 初始化为0
  try {
    shm_->Read(node_data, NODE_INFO_SIZE, TOPIC_INFO_SIZE);
    // NodesInfo *nodes = static_cast<NodesInfo *>(node_data);
    std::string jsonStr(node_data);

    // 检查是否为空或无效JSON
    if (jsonStr.empty() ||
        jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
      // 空数据，初始化默认值
      nodes_.nodes_count = 0;
      nodes_.alive_node_count = 0;
      return;
    }
    JsonValue json = JsonValue::deserialize(jsonStr);
    nodes_.nodes_count = json["node_count"].asInt();
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
  } catch (const std::exception& e) {
    // JSON解析失败，初始化默认值
    std::cout << "Failed to parse JSON from shared memory: " << e.what()
              << std::endl;
    nodes_.nodes_count = 0;
    nodes_.alive_node_count = 0;
  }
}

void ShmManager::readTopicsInfo() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  readTopicsInfo_();
}
void ShmManager::readTopicsInfoUnlocked() {
  char topic_data[TOPIC_INFO_SIZE];
  memset(topic_data, 0,
         TOPIC_INFO_SIZE);  // 初始化为0
  // std::cout << "readTopicsInfo" << std::endl;
  try {
    // std::cout << "shm_->Read" << std::endl;
    shm_->ReadUnlocked(topic_data, TOPIC_INFO_SIZE);
    // std::cout << "shm_->Read done" << std::endl;
    // TopicsInfo *topics = static_cast<TopicsInfo *>(topic_data);
    std::string jsonStr(topic_data);
    // std::cout << "jsonStr: " << jsonStr << std::endl;
    // 检查是否为空或无效JSON
    if (jsonStr.empty() ||
        jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
      // 空数据，初始化默认值
      // std::cout << "empty" << std::endl;
      topics_.topics_count = 0;
      return;
    }
    // std::cout << "not empty" << std::endl;
    JsonValue json = JsonValue::deserialize(jsonStr);
    // std::cout << "json: " << json.serialize() << std::endl;
    topics_.topics_count = json["topic_count"].asInt();
    // event_flag_ 已移除，不再从注册表读取（从 EventNotificationShm 读取）
    for (int i = 0; i < topics_.topics_count; i++) {
      topics_.topics[i].event_id_ = json["topics"][i]["topic_id"].asInt();
      std::strcpy(topics_.topics[i].name_,
                  json["topics"][i]["name"].asString().c_str());
    }
  } catch (const std::exception& e) {
    // JSON解析失败，初始化默认值
    std::cout << "Failed to parse JSON from shared memory: " << e.what()
              << std::endl;
    topics_.topics_count = 0;
  }
}
void ShmManager::readTopicsInfo_() {
  char topic_data[TOPIC_INFO_SIZE];
  memset(topic_data, 0,
         TOPIC_INFO_SIZE);  // 初始化为0
  // std::cout << "readTopicsInfo" << std::endl;
  try {
    // std::cout << "shm_->Read" << std::endl;
    shm_->Read(topic_data, TOPIC_INFO_SIZE);
    // std::cout << "shm_->Read done" << std::endl;
    // TopicsInfo *topics = static_cast<TopicsInfo *>(topic_data);
    std::string jsonStr(topic_data);
    // std::cout << "jsonStr: " << jsonStr << std::endl;
    // 检查是否为空或无效JSON
    if (jsonStr.empty() ||
        jsonStr.find_first_not_of(" \t\n\r") == std::string::npos) {
      // 空数据，初始化默认值
      // std::cout << "empty" << std::endl;
      topics_.topics_count = 0;
      return;
    }
    // std::cout << "not empty" << std::endl;
    JsonValue json = JsonValue::deserialize(jsonStr);
    // std::cout << "json: " << json.serialize() << std::endl;
    topics_.topics_count = json["topic_count"].asInt();
    // event_flag_ 已移除，不再从注册表读取（从 EventNotificationShm 读取）
    for (int i = 0; i < topics_.topics_count; i++) {
      topics_.topics[i].event_id_ = json["topics"][i]["topic_id"].asInt();
      std::strcpy(topics_.topics[i].name_,
                  json["topics"][i]["name"].asString().c_str());
    }
  } catch (const std::exception& e) {
    // JSON解析失败，初始化默认值
    std::cout << "Failed to parse JSON from shared memory: " << e.what()
              << std::endl;
    topics_.topics_count = 0;
  }
}
void ShmManager::initializeRegistry_() {
  // 初始化注册表直接从共享内存读取数据
  std::cout << "initializeRegistry" << std::endl;
  readTopicsInfo_();
  readNodesInfo_();
}
void ShmManager::updateNodeHeartbeat() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  nodes_.nodes[node_id_].last_heartbeat = time(nullptr);
  writeRegistryToShm_();
  // TBD心跳更新会频繁写入共享内存，后面优化为修改而非覆写
};

bool ShmManager::isNodeAlive() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  bool is_alive = nodes_.nodes[node_id_].is_alive;
  return is_alive;
};

void ShmManager::addNode(const NodeInfo& node_info) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  nodes_.nodes[node_id_] = node_info;
  nodes_.nodes_count++;
  nodes_.alive_node_count++;
  writeRegistryToShm_();
}

void ShmManager::updateNodeInfo(const NodeInfo& node_info) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  nodes_.nodes[node_id_] = node_info;
  writeRegistryToShm_();
};
void ShmManager::removeNode() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  //   nodes.erase(nodes.begin() + node_id);
  nodes_.nodes[node_id_].is_alive = false;
  nodes_.alive_node_count--;
  nodes_.nodes_count--;
  writeNodesInfo_();
}

void ShmManager::getNodeInfo(NodeInfo& node_info) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  node_info = nodes_.nodes[node_id_];
}
// TBD 这里的MAX_NODE_COUNT需要从配置文件中读取
int ShmManager::getNextNodeId() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  for (int i = 0; i < MAX_NODE_COUNT; i++) {
    if (!nodes_.nodes[i].is_alive) {
      return i;
    }
  }
  return -1;
}

int ShmManager::getAliveNodeCount() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  int count = nodes_.alive_node_count;
  return count;
}
int ShmManager::getNodeCount() {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  int count = nodes_.nodes_count;
  return count;
}

// void ShmManager::writeShmHead() {}
// 写入方法：使用进程内锁保护数据访问，使用 ShmBase 锁保护共享内存写入
void ShmManager::writeNodesInfo_() {
  // 1. 加进程内锁保护 nodes_ 的内存访问
  // std::lock_guard<std::mutex> lock(registry_mutex_);
  JsonValue json;
  json["node_count"] = nodes_.nodes_count;
  json["alive_node_count"] = nodes_.alive_node_count;
  json["nodes"] = JsonArray(nodes_.nodes_count);
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
  std::string json_str = json.serialize();
  // 2. 锁在 lock_guard 析构时自动释放
  // 3. 调用 Write() 写入共享内存（Write() 内部会用自己的锁保护）
  std::cout << json_str.c_str();
  shm_->Write(json_str.c_str(), json_str.size(), TOPIC_INFO_SIZE);
}

// void shmManager::updateEventFlag_(int event_id) {
//   std::lock_guard<std::mutex> lock(registry_mutex_);
//   topics_.event_flag_ |= (1 << event_id);
//   writeTopicsInfo_();
// }
// 写入方法：使用进程内锁保护数据访问，使用 ShmBase 锁保护共享内存写入
// 注意：event_flag_ 已移至独立的 EventNotificationShm，不再写入注册表
void ShmManager::writeTopicsInfo_() {
  // 1. 加进程内锁保护 topics_ 的内存访问
  // std::lock_guard<std::mutex> lock(registry_mutex_);
  JsonValue json;
  json["topic_count"] = topics_.topics_count;
  // event_flag_ 已移除，不再序列化
  json["topics"] = JsonArray(topics_.topics_count);
  for (int i = 0; i < topics_.topics_count; i++) {
    json["topics"][i]["topic_id"] = topics_.topics[i].event_id_;
    json["topics"][i]["name"] = std::string(topics_.topics[i].name_);
  }
  std::string json_str = json.serialize();
  // std::cout << "writeTopicsInfo: " << json_str << std::endl;
  // 2. 锁在 lock_guard 析构时自动释放
  // 3. 调用 Write() 写入共享内存（Write() 内部会用自己的锁保护）
  shm_->Write(json_str.c_str(), json_str.size());
}

void ShmManager::writeTopicsInfoUnlocked_() {
  // 1. 加进程内锁保护 topics_ 的内存访问
  // std::lock_guard<std::mutex> lock(registry_mutex_);
  JsonValue json;
  json["topic_count"] = topics_.topics_count;
  // event_flag_ 已移除，不再序列化
  json["topics"] = JsonArray(topics_.topics_count);
  for (int i = 0; i < topics_.topics_count; i++) {
    json["topics"][i]["topic_id"] = topics_.topics[i].event_id_;
    json["topics"][i]["name"] = std::string(topics_.topics[i].name_);
  }
  std::string json_str = json.serialize();
  // std::cout << "writeTopicsInfo: " << json_str << std::endl;
  // 2. 锁在 lock_guard 析构时自动释放
  // 3. 调用 Write() 写入共享内存（Write() 内部会用自己的锁保护）
  shm_->WriteUnlocked(json_str.c_str(), json_str.size());
}

void ShmManager::writeNodesInfoUnlocked_() {
  JsonValue json;
  json["node_count"] = nodes_.nodes_count;
  json["alive_node_count"] = nodes_.alive_node_count;
  json["nodes"] = JsonArray(nodes_.nodes_count);
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
  std::string json_str = json.serialize();
  std::cout << "writeNodesInfo: " << json_str << std::endl;
  shm_->WriteUnlocked(json_str.c_str(), json_str.size());
}

void ShmManager::writeRegistryToShm_() {
  // writeShmHead();
  // writeTopicsInfo() 和 writeNodesInfo() 内部已经有锁保护
  writeTopicsInfo_();
  writeNodesInfo_();
}

void ShmManager::writeRegistryToShmUnlocked_() {
  writeTopicsInfoUnlocked_();
  writeNodesInfoUnlocked_();
}

void ShmManager::addSubTopic(const std::string& topic_name,
                             const std::string& event_name) {
  // 使用进程内锁保护 nodes_ 和 topics_ 的访问
  std::lock_guard<std::mutex> lock(registry_mutex_);
  if (nodes_.nodes[node_id_].sub_topic_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(nodes_.nodes[node_id_]
                    .sub_topics[nodes_.nodes[node_id_].sub_topic_count],
                topic_name.c_str());
    nodes_.nodes[node_id_].sub_topic_count++;
  }
  // 查找或创建 topic event，findOrCreateTopicEvent_ 内部会处理 topics_ 的更新
  int event_id = findOrCreateTopicEvent_(topic_name, event_name);
  if (event_id < 0) {
    std::cerr << "Failed to create topic event" << std::endl;
  }
  writeRegistryToShm_();
  // 锁释放后，调用 writeTopicsInfo() 和 writeNodesInfo() 写入共享内存
  // 它们内部会重新加锁读取数据并写入
}

void ShmManager::addPubTopic(const std::string& topic_name,
                             const std::string& event_name) {
  // 使用进程内锁保护 nodes_ 和 topics_ 的访问
  std::lock_guard<std::mutex> lock(registry_mutex_);
  std::string full_name = topic_name + "_" + event_name;
  if (nodes_.nodes[node_id_].pub_topic_count < MAX_TOPICS_PER_NODE) {
    std::strcpy(nodes_.nodes[node_id_]
                    .pub_topics[nodes_.nodes[node_id_].pub_topic_count],
                full_name.c_str());
    nodes_.nodes[node_id_].pub_topic_count++;
  }
  // 查找或创建 topic event，findOrCreateTopicEvent_ 内部会处理 topics_ 的更新
  int event_id = findOrCreateTopicEvent_(topic_name, event_name);
  if (event_id < 0) {
    std::cerr << "Failed to create topic event" << std::endl;
  }
  TopicInfo topic_info;
  topic_info.event_id_ = event_id;
  // std::string full_name = topic_name + "_" + event_name;
  std::strcpy(topic_info.name_, full_name.c_str());
  topics_.topics[topics_.topics_count] = topic_info;
  topics_.topics_count++;
  std::cout << "event_id: " << event_id << std::endl;
  writeRegistryToShm_();
  // 锁释放后，调用 writeTopicsInfo() 和 writeNodesInfo() 写入共享内存
  // 它们内部会重新加锁读取数据并写入
}

void ShmManager::removeSubTopic(const std::string& topic_name,
                                const std::string& event_name) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
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

void ShmManager::removePubTopic(const std::string& topic_name,
                                const std::string& event_name) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
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
  std::lock_guard<std::mutex> lock(registry_mutex_);
  nodes_.nodes[node_id_].is_alive = true;
  writeRegistryToShm_();
}
void ShmManager::updateNodeName(const std::string& node_name) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  std::strcpy(nodes_.nodes[node_id_].node_name, node_name.c_str());
  writeRegistryToShm_();
}

void ShmManager::printRegistry() {
  // char data[MAX_SHM_MANGER_SIZE];
  // memset(data, 0, MAX_SHM_MANGER_SIZE); // 初始化为0
  // shm_->Read(data, MAX_SHM_MANGER_SIZE);
  // std::string jsonStr(data);
  // std::cout << jsonStr << std::endl;
  std::lock_guard<std::mutex> lock(registry_mutex_);
  // char topic_data[TOPIC_INFO_SIZE];
  // memset(topic_data, 0, TOPIC_INFO_SIZE);
  // shm_->Read(topic_data, TOPIC_INFO_SIZE);
  // std::string jsonStr(topic_data);
  // std::cout << "jsonStr: " << jsonStr << std::endl;
  // JsonValue json = JsonValue::deserialize(jsonStr);
  // std::cout << "topics_count: " << json["topic_count"].asInt() << std::endl;
  // std::cout << "event_flag_: " << json["event_flag_"].asInt() << std::endl;
  for (int i = 0; i < topics_.topics_count; i++) {
    std::cout << "name: " << topics_.topics->name_
              << " event_id: " << topics_.topics->event_id_ << std::endl;
  }
  for (int i = 0; i < nodes_.nodes_count; i++) {
    std::cout << "id: " << nodes_.nodes[i].node_id
              << " name: " << nodes_.nodes[i].node_name
              << " pid: " << nodes_.nodes[i].pid
              << " pub_count: " << nodes_.nodes[i].pub_topic_count
              << " sub_count: " << nodes_.nodes[i].sub_topic_count << std::endl;
  }
  // std::cout << "printRegistry" << std::endl;
}

// 查找或创建 topic+event 映射，返回 event_id（位索引）
int ShmManager::findOrCreateTopicEvent_(const std::string& topic_name,
                                        const std::string& event_name) {
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
  int new_event_id = topics_.topics_count + 1;
  topics_.topics[new_event_id].event_id_ = new_event_id;
  std::strcpy(topics_.topics[new_event_id].name_, full_name.c_str());
  topics_.topics_count++;

  return new_event_id;
}

// 注册 topic+event 组合，返回分配的 event_id（位索引）
int ShmManager::registerTopicEvent(const std::string& topic_name,
                                   const std::string& event_name) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  int event_id = findOrCreateTopicEvent_(topic_name, event_name);
  if (event_id >= 0) {
    writeTopicsInfo_();  // 更新到共享内存
  }
  return event_id;
}

// 查找 topic+event 对应的 event_id，如果不存在返回 -1
int ShmManager::getTopicEventId_(const std::string& topic_name,
                                 const std::string& event_name) {
  // std::lock_guard<std::mutex> lock(registry_mutex_);
  std::string full_name = topic_name + "_" + event_name;
  for (int i = 0; i < topics_.topics_count; i++) {
    if (std::string(topics_.topics[i].name_) == full_name) {
      int event_id = topics_.topics[i].event_id_;
      return event_id;
    }
  }
  return -1;
}

// 触发事件：设置对应的位并通知条件变量
void ShmManager::triggerEvent(const std::string& topic_name,
                              const std::string& event_name) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  // std::cout << "triggerEvent: " << topic_name << " " << event_name <<
  // std::endl;
  int event_id = getTopicEventId_(topic_name, event_name);
  if (event_id >= 0) {
    // std::cout << "triggerEventById: " << event_id << std::endl;
    triggerEventById_(event_id);
  }
}

// 触发事件（通过 event_id）
// 使用独立的事件通知共享内存，不再更新注册表
void ShmManager::triggerEventById_(int event_id) {
  if (event_id < 0 || event_id >= MAX_TOPICS_PER_NODE) {
    return;
  }

  // 直接使用事件通知共享内存触发事件（不需要更新注册表）
  event_notification_shm_->triggerEvent(event_id);
}

// 清除事件标志位
void ShmManager::clearTriggerEvent(int event_id) {
  // std::lock_guard<std::mutex> lock(registry_mutex_);
  if (event_id < 0 || event_id >= MAX_TOPICS_PER_NODE) {
    return;
  }
  event_notification_shm_->clearEvents(event_id);
}

// 清除所有事件标志位
void ShmManager::clearAllTriggerEvents() {
  // std::lock_guard<std::mutex> lock(registry_mutex_);
  event_notification_shm_->clearEvents();
  // writeTopicsInfo_();
}

// 批量读取并清除事件标志位（原子操作）
// 注意：readAndClearEventFlags 已废弃，使用 readAndClearEvents() 代替
// 如果需要清除特定的事件位，可以在读取后手动清除

bool ShmManager::isTopicExist(const std::string& topic_name,
                              const std::string& event_name) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  std::string full_name = topic_name + "_" + event_name;
  for (int i = 0; i < topics_.topics_count; i++) {
    if (std::string(topics_.topics[i].name_) == full_name) {
      return true;
    }
  }
  return false;
}