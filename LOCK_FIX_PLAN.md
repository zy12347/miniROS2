# 共享内存锁修复方案

## 问题根源

所有方法都使用 `std::lock_guard` 作为局部变量，导致锁在函数返回时被释放，无法保持锁的状态。

## 修复方案

### 方案1：使用 `std::unique_lock` 返回锁守卫（推荐）

**修改 `ShmManager` 的锁管理方法**：

```cpp
// 返回锁守卫，调用者持有其生命周期
std::unique_lock<std::mutex> shmManagerLock() { 
  auto lock = std::unique_lock<std::mutex>(registry_mutex_);
  shm_->shmBaseLock();
  return lock;  // 移动语义，返回锁守卫
}

// 不加锁版本（假设调用者已持有锁）
void shmManagerWaitUnlocked() {
  shm_->shmBaseWait();  // 假设已持有 registry_mutex_ 和 shmBaseLock()
}

int getTriggerEventUnlocked() {
  return topics_.event_flag_;  // 假设已持有 registry_mutex_
}
```

**修改 `spinLoop()`**：

```cpp
void Node::spinLoop() {
    while (spinning_) {
        // 获取锁守卫，保持锁的生命周期
        auto registry_lock = shm_manager_->shmManagerLock();
        
        // 等待条件变量（已持有锁）
        shm_manager_->shmManagerWaitUnlocked();
        
        // 读取事件标志位（已持有锁）
        int trigger_event = shm_manager_->getTriggerEventUnlocked();
        
        // 收集需要处理的订阅者
        std::vector<size_t> subscribers_to_execute;
        std::vector<int> event_ids_to_clear;
        {
            std::lock_guard<std::mutex> lock(node_mutex_);
            for (size_t id = 0; id < subscriptions_.size() && id < subscription_event_ids_.size(); id++) {
                int event_id = subscription_event_ids_[id];
                if (event_id >= 0 && event_id < 32) {
                    if (trigger_event & (1 << event_id)) {
                        subscribers_to_execute.push_back(id);
                        event_ids_to_clear.push_back(event_id);
                    }
                }
            }
        }
        
        // 批量清除事件标志位（已持有锁）
        if (!event_ids_to_clear.empty()) {
            shm_manager_->readAndClearEventFlagsUnlocked(event_ids_to_clear);
        }
        
        // 先解锁 shmBase
        shm_manager_->shmBaseUnlock();
        
        // registry_lock 析构时自动释放 registry_mutex_
        // 锁守卫自动释放锁（registry_lock 析构）
        
        // 在无锁状态下执行回调
        std::lock_guard<std::mutex> lock(node_mutex_);
        for (size_t id : subscribers_to_execute) {
            if (id < subscriptions_.size()) {
                subscriptions_[id]->execute();
            }
        }
    }
}
```

### 方案2：使用 RAII 锁管理类（更清晰）

**创建 `ShmManagerLockGuard` 类**：

```cpp
class ShmManagerLockGuard {
public:
  ShmManagerLockGuard(ShmManager* shm_mgr) 
    : shm_mgr_(shm_mgr), 
      registry_lock_(shm_mgr->getRegistryMutex()) {
    shm_mgr_->shmBaseLock();
  }
  
  ~ShmManagerLockGuard() {
    shm_mgr_->shmBaseUnlock();
  }
  
  // 等待条件变量
  void wait() {
    shm_mgr_->shmBaseWait();
  }
  
  // 获取事件标志位
  int getTriggerEvent() {
    return shm_mgr_->getTopics().event_flag_;
  }
  
  // 清除事件标志位
  int readAndClearEventFlags(const std::vector<int> &event_ids) {
    return shm_mgr_->readAndClearEventFlagsUnlocked(event_ids);
  }
  
private:
  ShmManager* shm_mgr_;
  std::unique_lock<std::mutex> registry_lock_;
};
```

**修改 `spinLoop()`**：

```cpp
void Node::spinLoop() {
    while (spinning_) {
        {
            ShmManagerLockGuard lock_guard(shm_manager_.get());
            lock_guard.wait();  // 等待条件变量
            
            int trigger_event = lock_guard.getTriggerEvent();
            
            // 收集需要处理的订阅者
            std::vector<size_t> subscribers_to_execute;
            std::vector<int> event_ids_to_clear;
            {
                std::lock_guard<std::mutex> lock(node_mutex_);
                for (size_t id = 0; id < subscriptions_.size() && id < subscription_event_ids_.size(); id++) {
                    int event_id = subscription_event_ids_[id];
                    if (event_id >= 0 && event_id < 32) {
                        if (trigger_event & (1 << event_id)) {
                            subscribers_to_execute.push_back(id);
                            event_ids_to_clear.push_back(event_id);
                        }
                    }
                }
            }
            
            // 批量清除事件标志位
            if (!event_ids_to_clear.empty()) {
                lock_guard.readAndClearEventFlags(event_ids_to_clear);
            }
            
            // lock_guard 析构时自动释放所有锁
        }
        
        // 在无锁状态下执行回调
        std::lock_guard<std::mutex> lock(node_mutex_);
        for (size_t id : subscribers_to_execute) {
            if (id < subscriptions_.size()) {
                subscriptions_[id]->execute();
            }
        }
    }
}
```

## 关键修复点

1. ✅ **锁的生命周期管理**：使用 `std::unique_lock` 或 RAII 类管理锁
2. ✅ **提供不加锁的内部版本**：供已持有锁的调用者使用
3. ✅ **确保 `pthread_cond_wait()` 正确使用**：在持有锁的情况下调用

## 优先级

### P0（立即修复）
1. 修复 `shmManagerLock()` 的锁生命周期问题
2. 修复 `shmManagerWait()` 的锁使用问题
3. 确保 `pthread_cond_wait()` 在持有锁的情况下调用

### P1（重要）
4. 提供不加锁的内部版本方法
5. 统一锁管理策略


