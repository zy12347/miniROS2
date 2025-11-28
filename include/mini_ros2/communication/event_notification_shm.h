#pragma once
#include <pthread.h>
#include <stdint.h>

#include <bitset>
#include <memory>
#include <stdexcept>
#include <string>

#include "mini_ros2/communication/shared_memory.h"

#define EVENT_NOTIFICATION_SHM_NAME "/miniros2_event_notification"
#define EVENT_NOTIFICATION_SHM_SIZE sizeof(EventNotificationData)
#define EVENT_MAX_PUB_COUNT 64
#define EVENT_MAX_SYNC_COUNT 64
#define EVENT_MAX_COUNT (EVENT_MAX_PUB_COUNT + EVENT_MAX_SYNC_COUNT)              // 位数
// 事件通知共享内存数据结构
struct EventNotificationData {
  uint32_t
      initialized_;  // 初始化标志：0x4556454E = "EVEN" (Event Notification)
  pthread_mutex_t mutex_;  // 互斥锁（进程间共享）
  pthread_cond_t cond_;    // 条件变量（进程间共享）
  // pthread_cond_t cond_req_;    // 条件变量（进程间共享）
  pthread_cond_t cond_res_;    // 条件变量（进程间共享）
  // uint32_t event_flag_;    // 事件标志位（第i位表示第i个事件）
  std::bitset<EVENT_MAX_COUNT> event_flag_;
  // std::bitset<EVENT_MAX_COUNT> event_flag_req_;
  std::bitset<EVENT_MAX_COUNT> event_flag_res_;
  int event_count_list_[EVENT_MAX_COUNT];
  uint64_t time_;  // 时间戳
  int32_t ref_count_;  // 引用计数（进程间共享）
  // char padding_[EVENT_NOTIFICATION_SHM_SIZE - sizeof(uint32_t) -
  //               sizeof(pthread_mutex_t) - sizeof(pthread_cond_t) -
  //               sizeof(std::bitset<EVENT_MAX_COUNT>) -
  //               sizeof(uint64_t) - sizeof(int32_t)];  // 填充到固定大小
};

// 独立的事件通知共享内存管理类
class EventNotificationShm {
 public:
  EventNotificationShm();
  ~EventNotificationShm();

  // 初始化共享内存（创建者调用）
  void Create();

  // 打开已存在的共享内存
  void Open();

  // 检查共享内存是否存在
  bool Exists() const;

  // 触发事件：设置对应的位并通知条件变量
  void triggerEvent(int event_id);

  // 等待事件（带超时），返回当前的事件标志位
  std::bitset<EVENT_MAX_COUNT> waitForEvent(uint64_t timeout_ms);

  void triggerEventResponse(int event_id);

  std::bitset<EVENT_MAX_COUNT> waitForEventResponse(uint64_t timeout_ms);

  // 读取并清除事件标志位（原子操作）
  std::bitset<EVENT_MAX_COUNT> readAndClearEvents();

  // 读取事件标志位（不清除）
  std::bitset<EVENT_MAX_COUNT> readEvents() const;

  // 清除事件标志位
  void clearEvents();

  void clearEvents(int event_id);

  void decreaseEventCount(int event_id);

  // 事件ID类型判断和验证
  // 判断 event_id 是否为 pub 事件（范围 [0, EVENT_MAX_PUB_COUNT-1]）
  static bool isPubEvent(int event_id) {
    return event_id >= 0 && event_id < EVENT_MAX_PUB_COUNT;
  }

  // 判断 event_id 是否为 service 事件（范围 [EVENT_MAX_PUB_COUNT, EVENT_MAX_COUNT-1]）
  static bool isServiceEvent(int event_id) {
    return event_id >= EVENT_MAX_PUB_COUNT && event_id < EVENT_MAX_COUNT;
  }

  // 验证 event_id 是否有效
  static bool isValidEventId(int event_id) {
    return event_id >= 0 && event_id < EVENT_MAX_COUNT;
  }

  // 唤醒所有等待的线程（用于退出时唤醒）
  void notifyAll();

  // 获取锁（用于需要手动管理锁的场景）
  void lock();

  // 释放锁
  void unlock();

 private:
  // 初始化互斥锁和条件变量
  void initMutexAndCond();

  // 缓存指针
  void cachePointers();

  std::shared_ptr<SharedMemory> shm_;
  EventNotificationData* data_ptr_ = nullptr;
  pthread_mutex_t* mutex_ptr_ = nullptr;
  pthread_cond_t* cond_ptr_ = nullptr;
  // pthread_cond_t* cond_req_ptr_ = nullptr;
  pthread_cond_t* cond_res_ptr_ = nullptr;
  std::bitset<EVENT_MAX_COUNT>* event_flag_ptr_ = nullptr;
  // std::bitset<EVENT_MAX_COUNT>* event_flag_req_ptr_ = nullptr;
  std::bitset<EVENT_MAX_COUNT>* event_flag_res_ptr_ = nullptr;

  int* event_count_list_ptr_ = nullptr;

  int32_t* ref_count_ptr_ = nullptr;  // 引用计数指针
  bool is_owner_ = false;

  // 引用计数管理
  void incrementRefCount();
  void decrementRefCount();
};
