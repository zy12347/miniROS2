#include "mini_ros2/communication/shm_manager.h"
#include "mini_ros2/logger.h"
#include <sys/mman.h>
#include <cstring>
#include <ctime>
#include <chrono>
#include <mutex>

// 静态成员变量定义（双层指针构造）
ShmManager* ShmManager::instance_ = nullptr;
std::mutex ShmManager::creat_mutex_;

ShmManager::ShmManager() {
  LOGD("shm_manager");
  shm_ = std::make_shared<SharedMemory>(SHM_MANAGER_NAME, SHM_MANAGER_SIZE);

  // 初始化事件通知共享内存
  event_notification_shm_ = std::make_shared<EventNotificationShm>();
  if (event_notification_shm_->Exists()) {
    event_notification_shm_->Open();
    LOGD("event_notification_shm open");
  } else {
    event_notification_shm_->Create();
    LOGD("event_notification_shm create");
  }

  // 检查共享内存是否已存在
  if (shm_->Exists()) {
    // 已存在，直接打开
    if (!shm_->Open()) {
      throw std::runtime_error("Failed to open shared memory");
    }
    LOGD("shm_manager open");
    Open();
    // initializeRegistry_();  // 只有事先已存在共享内存信息才尝试拷贝注册表信息
  } else {
    // 不存在，创建新的
    LOGD("shm_manager create");
    if (!shm_->Create()) {
      throw std::runtime_error("Failed to create shared memory");
    }
    if (!shm_->Open()) {
      throw std::runtime_error("Failed to open shared memory");
    }
    is_owner_ = true;
    initMutexAndCond();
  }
};

ShmManager::~ShmManager() {
  LOGD("ShmManager destructor: cleaning up shared memory");

  // 清理事件通知共享内存
  if (event_notification_shm_) {
    // EventNotificationShm 的析构函数会自动清理
    event_notification_shm_.reset();
  }

  // 清理注册表共享内存
  if (shm_) {
    // 减少引用计数
    decrementRefCount();
    
    // 打印当前引用计数
    if (ref_count_ptr_ && mutex_ptr_) {
      int ret = pthread_mutex_lock(mutex_ptr_);
      if (ret == 0) {
        LOGD("ShmManager destructor: node exiting, current ref_count = " 
             << *ref_count_ptr_);
        pthread_mutex_unlock(mutex_ptr_);
      }
    }
    
    // 如果引用计数为0，清除共享内存
    if (ref_count_ptr_ && *ref_count_ptr_ == 0) {
      LOGD("ShmManager destructor: last node, cleaning up shared memory "
           << SHM_MANAGER_NAME);
      shm_->Unlink();
    }
    shm_->Close();
    shm_.reset();
  }
}

// void ShmManager::readNodesInfo_() {
//   if (nodes_info_ptr_ == nullptr) {
//     throw std::runtime_error("Shared memory not initialized");
//   }

//   if (mutex_ptr_ == nullptr) {
//     throw std::runtime_error("Mutex not initialized");
//   }

//   // 获取锁
//   int ret = pthread_mutex_lock(mutex_ptr_);
//   if (ret != 0) {
//     throw std::runtime_error("Failed to lock mutex: " +
//                              std::string(strerror(ret)));
//   }

//   try {
//     std::cout << "read node info_" << std::endl;
//     // 直接从共享内存读取
//     nodes_.nodes_count = nodes_info_ptr_->nodes_count;
//     nodes_.alive_node_count = nodes_info_ptr_->alive_node_count;
//     for (int i = 0; i < nodes_.nodes_count && i < MAX_NODE_COUNT; i++) {
//       nodes_.nodes[i] = nodes_info_ptr_->nodes[i];
//     }
//   } catch (...) {
//     pthread_mutex_unlock(mutex_ptr_);
//     throw;
//   }

//   // 释放锁
//   ret = pthread_mutex_unlock(mutex_ptr_);
//   if (ret != 0) {
//     throw std::runtime_error("Failed to unlock mutex: " +
//                              std::string(strerror(ret)));
//   }
// }

// void ShmManager::readTopicsInfo() {
//   std::lock_guard<std::mutex> lock(registry_mutex_);
//   readTopicsInfo_();
// }

// void ShmManager::readTopicsInfoUnlocked() {
//   if (topics_info_ptr_ == nullptr) {
//     throw std::runtime_error("Shared memory not initialized");
//   }

//   if (mutex_ptr_ == nullptr) {
//     throw std::runtime_error("Mutex not initialized");
//   }

//   // 检查是否已持有锁（这里假设调用者已经持有锁，但为了安全起见，我们仍然需要锁）
//   // 注意：这个方法名是 Unlocked，但实际上我们仍然需要锁来保证数据一致性
//   // 如果调用者已经持有锁，这里会死锁，所以这个方法应该被重新设计
//   // 暂时保留锁，但建议重构
//   int ret = pthread_mutex_lock(mutex_ptr_);
//   if (ret != 0) {
//     throw std::runtime_error("Failed to lock mutex: " +
//                              std::string(strerror(ret)));
//   }

//   try {
//     // 直接从共享内存读取
//     topics_.topics_count = topics_info_ptr_->topics_count;
//     for (int i = 0; i < topics_.topics_count && i < MAX_TOPICS_PER_NODE; i++) {
//       topics_.topics[i] = topics_info_ptr_->topics[i];
//     }
//   } catch (...) {
//     pthread_mutex_unlock(mutex_ptr_);
//     throw;
//   }

//   // 释放锁
//   ret = pthread_mutex_unlock(mutex_ptr_);
//   if (ret != 0) {
//     throw std::runtime_error("Failed to unlock mutex: " +
//                              std::string(strerror(ret)));
//   }
// }

// void ShmManager::readTopicsInfo_() {
//   if (topics_info_ptr_ == nullptr) {
//     throw std::runtime_error("Shared memory not initialized");
//   }

//   if (mutex_ptr_ == nullptr) {
//     throw std::runtime_error("Mutex not initialized");
//   }

//   // 获取锁
//   int ret = pthread_mutex_lock(mutex_ptr_);
//   if (ret != 0) {
//     throw std::runtime_error("Failed to lock mutex: " +
//                              std::string(strerror(ret)));
//   }

//   try {
//     // 直接从共享内存读取
//     topics_.topics_count = topics_info_ptr_->topics_count;
//     for (int i = 0; i < topics_.topics_count && i < MAX_TOPICS_PER_NODE; i++) {
//       topics_.topics[i] = topics_info_ptr_->topics[i];
//     }
//   } catch (...) {
//     pthread_mutex_unlock(mutex_ptr_);
//     throw;
//   }

//   // 释放锁
//   ret = pthread_mutex_unlock(mutex_ptr_);
//   if (ret != 0) {
//     throw std::runtime_error("Failed to unlock mutex: " +
//                              std::string(strerror(ret)));
//   }
// }
// void ShmManager::initializeRegistry_() {
//   // 初始化注册表直接从共享内存读取数据
//   std::cout << "initializeRegistry" << std::endl;
//   readTopicsInfo_();
//   readNodesInfo_();
// }

void ShmManager::Open() {
  if (!shm_->Open()) {
    throw std::runtime_error("Failed to open shared memory");
  }

  ShmManagerData* head =
      static_cast<ShmManagerData*>(shm_->Data());
  if (!head) {
    throw std::runtime_error(
        "Failed to get shared memory pointer");
  }

  // 检查是否已初始化
  if (is_owner_ || head->initialized_ != 0x4D525332) {  // "MRS2"
    initMutexAndCond();
  } else {
    cachePointers();
    // 增加引用计数（非创建者）
    incrementRefCount();
  }
}

void ShmManager::initMutexAndCond() {
  ShmManagerData* head =
      static_cast<ShmManagerData*>(shm_->Data());
  if (!head) {
    throw std::runtime_error(
        "Failed to get shared memory pointer");
  }

  // 初始化互斥锁
  pthread_mutexattr_t mutex_attr;
  int ret = pthread_mutexattr_init(&mutex_attr);
  if (ret != 0) {
    throw std::runtime_error("Failed to init mutex attr: " +
                             std::string(strerror(ret)));
  }

  ret = pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    throw std::runtime_error("Failed to set mutex shared: " +
                             std::string(strerror(ret)));
  }

  ret = pthread_mutex_init(&head->mutex_, &mutex_attr);
  if (ret != 0) {
    pthread_mutexattr_destroy(&mutex_attr);
    throw std::runtime_error("Failed to init mutex: " +
                             std::string(strerror(ret)));
  }
  pthread_mutexattr_destroy(&mutex_attr);

  // 初始化条件变量
  pthread_condattr_t cond_attr;
  ret = pthread_condattr_init(&cond_attr);
  if (ret != 0) {
    throw std::runtime_error("Failed to init cond attr: " +
                             std::string(strerror(ret)));
  }

  ret = pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("Failed to set cond shared: " +
                             std::string(strerror(ret)));
  }

  ret = pthread_cond_init(&head->cond_, &cond_attr);
  if (ret != 0) {
    pthread_condattr_destroy(&cond_attr);
    throw std::runtime_error("Failed to init cond: " +
                             std::string(strerror(ret)));
  }
  pthread_condattr_destroy(&cond_attr);

  // 设置初始化标志
  head->initialized_ = 0x4D525332;  // "MRS2"
  head->time_ = 0;
  head->ref_count_ = 1;  // 创建者初始化为1
  // 初始化数据结构
  head->topics_info_.topics_count = 0;
  head->nodes_info_.nodes_count = 0;
  head->nodes_info_.alive_node_count = 0;

  cachePointers();
}

void ShmManager::cachePointers() {
  ShmManagerData* head =
      static_cast<ShmManagerData*>(shm_->Data());
  if (!head) {
    throw std::runtime_error(
        "Failed to get shared memory pointer");
  }

  data_ptr_ = head;
  mutex_ptr_ = &head->mutex_;
  cond_ptr_ = &head->cond_;
  time_ptr_ = &head->time_;
  ref_count_ptr_ = &head->ref_count_;
  topics_info_ptr_ = &head->topics_info_;
  nodes_info_ptr_ = &head->nodes_info_;
}

// void ShmManager::syncRegistryFromShm() {
//   std::lock_guard<std::mutex> lock(registry_mutex_);
//   std::cout << "syncRegistry " << std::endl;
//   readTopicsInfo_();
//   std::cout << "read node info" << std::endl;
//   readNodesInfo_();
// }
void ShmManager::updateNodeHeartbeat() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      nodes_info_ptr_->nodes[node_id_].last_heartbeat = time(nullptr);
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
};

bool ShmManager::isNodeAlive() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  bool is_alive = false;
  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      is_alive = nodes_info_ptr_->nodes[node_id_].is_alive;
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return is_alive;
};

void ShmManager::addNode(const NodeInfo& node_info) {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      nodes_info_ptr_->nodes[node_id_] = node_info;
      nodes_info_ptr_->nodes_count++;
      nodes_info_ptr_->alive_node_count++;
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}

void ShmManager::updateNodeInfo(const NodeInfo& node_info) {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      nodes_info_ptr_->nodes[node_id_] = node_info;
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
};
void ShmManager::removeNode() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      nodes_info_ptr_->nodes[node_id_].is_alive = false;
      nodes_info_ptr_->alive_node_count--;
      nodes_info_ptr_->nodes_count--;
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  triggerEventById_(MAX_TOPICS_PER_NODE - 1);
}

void ShmManager::getNodeInfo(NodeInfo& node_info) {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      node_info = nodes_info_ptr_->nodes[node_id_];
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}
// TBD 这里的MAX_NODE_COUNT需要从配置文件中读取
int ShmManager::getNextNodeId() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  int next_id = -1;
  try {
  for (int i = 0; i < MAX_NODE_COUNT; i++) {
      if (!nodes_info_ptr_->nodes[i].is_alive) {
        next_id = i;
        break;
      }
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return next_id;
}

int ShmManager::getAliveNodeCount() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  int count = 0;
  try {
    count = nodes_info_ptr_->alive_node_count;
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return count;
}

int ShmManager::getNodeCount() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  int count = 0;
  try {
    count = nodes_info_ptr_->nodes_count;
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return count;
}

// // writeNodesInfo_ 已废弃，现在直接操作共享内存指针
// void ShmManager::writeNodesInfo_() {
//   // 此方法已废弃，所有操作都直接通过指针进行
// }

// // writeTopicsInfo_ 已废弃，现在直接操作共享内存指针
// void ShmManager::writeTopicsInfo_() {
//   // 此方法已废弃，所有操作都直接通过指针进行
// }

// void ShmManager::writeTopicsInfoUnlocked_() {
//   // 此方法已废弃，所有操作都直接通过指针进行
// }

// void ShmManager::writeNodesInfoUnlocked_() {
//   // 此方法已废弃，所有操作都直接通过指针进行
// }

// void ShmManager::writeRegistryToShm_() {
//   // 此方法已废弃，所有操作都直接通过指针进行
// }

// void ShmManager::writeRegistryToShmUnlocked_() {
//   // 此方法已废弃，所有操作都直接通过指针进行
// }

void ShmManager::addSubTopic(const std::string& topic_name,
                             const std::string& event_name) {
  if (nodes_info_ptr_ == nullptr || topics_info_ptr_ == nullptr ||
      mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      if (nodes_info_ptr_->nodes[node_id_].sub_topic_count <
          MAX_TOPICS_PER_NODE) {
        nodes_info_ptr_->nodes[node_id_].sub_topic_count++;
      }
    }
    // 查找或创建 topic event
    int event_id = findOrCreateTopicEventUnlocked_(topic_name, event_name);
  if (event_id < 0) {
    LOGE("Failed to create topic event");
  }
    // 更新时间戳
    *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}

void ShmManager::addPubTopic(const std::string& topic_name,
                             const std::string& event_name) {
  if (nodes_info_ptr_ == nullptr || topics_info_ptr_ == nullptr ||
      mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
  std::string full_name = topic_name + "_" + event_name;
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      if (nodes_info_ptr_->nodes[node_id_].pub_topic_count <
          MAX_TOPICS_PER_NODE) {
        nodes_info_ptr_->nodes[node_id_].pub_topic_count++;
      }
    }
    // 查找或创建 topic event
    int event_id = findOrCreateTopicEventUnlocked_(topic_name, event_name);
  if (event_id < 0) {
    LOGE("Failed to create topic event");
    } else {
      // 添加到 topics 列表
      if (topics_info_ptr_->topics_count < MAX_TOPICS_PER_NODE) {
  TopicInfo topic_info;
  topic_info.event_id_ = event_id;
  std::strcpy(topic_info.name_, full_name.c_str());
        topics_info_ptr_->topics[topics_info_ptr_->topics_count] = topic_info;
        topics_info_ptr_->topics_count++;
        LOGD("event_id: " << event_id);
      }
    }
    // 更新时间戳
    *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}

void ShmManager::addSyncTopic(const std::string& topic_name, const std::string& event_name) {
    if (nodes_info_ptr_ == nullptr || topics_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
        throw std::runtime_error("Shared memory not initialized");
    }

    // 获取锁
    int ret = pthread_mutex_lock(mutex_ptr_);
    if (ret != 0) {
        throw std::runtime_error("Failed to lock mutex: " + std::string(strerror(ret)));
    }

    try {
        std::string full_name = topic_name + "_" + event_name;
        if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
            if (nodes_info_ptr_->nodes[node_id_].sync_topic_count < MAX_TOPICS_PER_NODE) {
                nodes_info_ptr_->nodes[node_id_].sync_topic_count++;
            }
        }
        // 查找或创建 topic event
        int event_id = findOrCreateTopicEventUnlocked_(topic_name, event_name);
        if (event_id < 0) {
            LOGE("Failed to create topic event");
        } else {
            // 添加到 topics 列表
            if (topics_info_ptr_->topics_count < MAX_TOPICS_PER_NODE) {
                TopicInfo topic_info;
                topic_info.event_id_ = event_id;
                std::strcpy(topic_info.name_, full_name.c_str());
                topics_info_ptr_->topics[topics_info_ptr_->topics_count] = topic_info;
                topics_info_ptr_->topics_count++;
                LOGD("event_id: " << event_id);
            }
        }
        // 更新时间戳
        *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
    } catch (...) {
        pthread_mutex_unlock(mutex_ptr_);
        throw;
    }

    // 释放锁
    ret = pthread_mutex_unlock(mutex_ptr_);
    if (ret != 0) {
        throw std::runtime_error("Failed to unlock mutex: " + std::string(strerror(ret)));
    }
}
// void ShmManager::removeSubTopic(const std::string& topic_name,
//                                 const std::string& event_name) {
//   std::lock_guard<std::mutex> lock(registry_mutex_);
// }

// void ShmManager::removePubTopic(const std::string& topic_name,
//                                 const std::string& event_name) {
//   std::lock_guard<std::mutex> lock(registry_mutex_);
//   // for (int i = 0; i < nodes_.nodes[node_id_].pub_topic_count; i++) {
//   //   if (nodes_.nodes[node_id_].pub_topics[i] == topic_name) {
//   //     // 移除topic，将后面的元素前移
//   //     // for (int j = i; j < nodes_.nodes[node_id_].pub_topic_count - 1; j++)
//   //     {
//   //     //   std::strcpy(nodes_.nodes[node_id_].pub_topics[j],
//   //     //               nodes_.nodes[node_id_].pub_topics[j + 1]);
//   //     // }
//   //     nodes_.nodes[node_id_].pub_topic_count--;
//   //     writeRegistryToShm_();
//   //     break;
//   //   }
//   // }
// }

void ShmManager::updateNodeAlive() {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      nodes_info_ptr_->nodes[node_id_].is_alive = true;
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}

void ShmManager::updateNodeName(const std::string& node_name) {
  if (nodes_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    if (node_id_ >= 0 && node_id_ < MAX_NODE_COUNT) {
      std::strcpy(nodes_info_ptr_->nodes[node_id_].node_name,
                  node_name.c_str());
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}

void ShmManager::printRegistry() {
  if (topics_info_ptr_ == nullptr || nodes_info_ptr_ == nullptr ||
      mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  try {
    for (int i = 0; i < topics_info_ptr_->topics_count; i++) {
      LOGD("name: " << topics_info_ptr_->topics[i].name_
           << " event_id: " << topics_info_ptr_->topics[i].event_id_);
    }
    for (int i = 0; i < nodes_info_ptr_->nodes_count; i++) {
      LOGD("id: " << nodes_info_ptr_->nodes[i].node_id
           << " name: " << nodes_info_ptr_->nodes[i].node_name
           << " pid: " << nodes_info_ptr_->nodes[i].pid
           << " pub_count: " << nodes_info_ptr_->nodes[i].pub_topic_count
           << " sub_count: " << nodes_info_ptr_->nodes[i].sub_topic_count);
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }
}

// 查找或创建 topic+event 映射，返回 event_id（位索引）
// 注意：此方法假设调用者已经持有 mutex_ptr_ 锁
int ShmManager::findOrCreateTopicEventUnlocked_(const std::string& topic_name,
                                        const std::string& event_name) {
  if (topics_info_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  std::string full_name = topic_name + "_" + event_name;

  // 查找是否已存在
  for (int i = 0; i < topics_info_ptr_->topics_count; i++) {
    if (std::string(topics_info_ptr_->topics[i].name_) == full_name) {
      return topics_info_ptr_->topics[i].event_id_;
    }
  }

  // 不存在，创建新的映射
  if (topics_info_ptr_->topics_count >= MAX_TOPICS_PER_NODE) {
    LOGE("Maximum topic count reached");
    return -1;
  }

  // 分配新的 event_id（位索引）
  int new_event_id = topics_info_ptr_->topics_count + 1;
  topics_info_ptr_->topics[topics_info_ptr_->topics_count].event_id_ =
      new_event_id;
  std::strcpy(topics_info_ptr_->topics[topics_info_ptr_->topics_count].name_,
              full_name.c_str());
  topics_info_ptr_->topics_count++;

  return new_event_id;
}

// 查找或创建 topic+event 映射，返回 event_id（位索引）
int ShmManager::findOrCreateTopicEvent_(const std::string& topic_name,
                                        const std::string& event_name) {
  if (topics_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  int event_id = -1;
  try {
    event_id = findOrCreateTopicEventUnlocked_(topic_name, event_name);
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return event_id;
}

// 注册 topic+event 组合，返回分配的 event_id（位索引）
int ShmManager::registerTopicEvent(const std::string& topic_name,
                                   const std::string& event_name) {
  if (topics_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  int event_id = -1;
  try {
    event_id = findOrCreateTopicEventUnlocked_(topic_name, event_name);
  if (event_id >= 0) {
      // 更新时间戳
      *time_ptr_ = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return event_id;
}

// 查找 topic+event 对应的 event_id，如果不存在返回 -1（公共方法）
int ShmManager::getTopicEventId(const std::string& topic_name,
                                const std::string& event_name) {
  return getTopicEventId_(topic_name, event_name);
}

// 查找 topic+event 对应的 event_id，如果不存在返回 -1（私有方法）
int ShmManager::getTopicEventId_(const std::string& topic_name,
                                 const std::string& event_name) {
  if (topics_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  int event_id = -1;
  try {
  std::string full_name = topic_name + "_" + event_name;
    for (int i = 0; i < topics_info_ptr_->topics_count; i++) {
      if (std::string(topics_info_ptr_->topics[i].name_) == full_name) {
        event_id = topics_info_ptr_->topics[i].event_id_;
        break;
      }
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return event_id;
}

void ShmManager::triggerEventResponse(const std::string& topic_name,
                                      const std::string& event_name) {
  int event_id = getTopicEventId_(topic_name, event_name);
  if (event_id >= 0) {
    event_notification_shm_->triggerEventResponse(event_id);
  }
}

// 触发事件：设置对应的位并通知条件变量
void ShmManager::triggerEvent(const std::string& topic_name,
                              const std::string& event_name) {
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
  if (topics_info_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    throw std::runtime_error("Shared memory not initialized");
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to lock mutex: " +
                             std::string(strerror(ret)));
  }

  bool exists = false;
  try {
  std::string full_name = topic_name + "_" + event_name;
    for (int i = 0; i < topics_info_ptr_->topics_count; i++) {
      if (std::string(topics_info_ptr_->topics[i].name_) == full_name) {
        exists = true;
        break;
      }
    }
  } catch (...) {
    pthread_mutex_unlock(mutex_ptr_);
    throw;
  }

  // 释放锁
  ret = pthread_mutex_unlock(mutex_ptr_);
  if (ret != 0) {
    throw std::runtime_error("Failed to unlock mutex: " +
                             std::string(strerror(ret)));
  }

  return exists;
}

void ShmManager::incrementRefCount() {
  if (ref_count_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    return;  // 未初始化
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    return;  // 获取锁失败，忽略错误
  }

  (*ref_count_ptr_)++;
  LOGD("ShmManager ref_count incremented to: " << *ref_count_ptr_);

  // 释放锁
  pthread_mutex_unlock(mutex_ptr_);
}

void ShmManager::decrementRefCount() {
  if (ref_count_ptr_ == nullptr || mutex_ptr_ == nullptr) {
    return;  // 未初始化
  }

  // 获取锁
  int ret = pthread_mutex_lock(mutex_ptr_);
  if (ret != 0) {
    return;  // 获取锁失败，忽略错误
  }

  if (*ref_count_ptr_ > 0) {
    (*ref_count_ptr_)--;
    LOGD("ShmManager ref_count decremented to: " << *ref_count_ptr_);
  }

  // 释放锁
  pthread_mutex_unlock(mutex_ptr_);
}