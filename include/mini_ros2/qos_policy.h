#pragma once

struct QosPolicy {
  // 历史策略：消息缓存策略
  // KEEP_LAST: 保留最后 N 条消息（由 history_depth 指定，使用环形缓冲区覆盖旧数据）
  // KEEP_ALL: 保留所有消息直到被读取（需要更多内存，适用于不能丢失消息的场景）
  enum History { KEEP_LAST, KEEP_ALL };
  
  QosPolicy() : history(KEEP_LAST), history_depth(1) {}

  QosPolicy(QosPolicy &qos_policy) : history(qos_policy.history), history_depth(qos_policy.history_depth) {}

  QosPolicy(History history, int history_depth) : history(history), history_depth(history_depth) {}
  
  History history;
  
  // 历史深度：保留的消息数量
  // KEEP_LAST 模式：保留最后 history_depth 条消息（环形缓冲区）
  // KEEP_ALL 模式：最多保留 history_depth 条消息（资源限制）
  // 也用于计算共享内存大小：data_max_size = single_message_size * history_depth
  int history_depth;
};