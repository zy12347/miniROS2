#include "mini_ros2/node.h"
Node::Node(const std::string &node_name, const std::string &name_space,
           int domain_id)
    : node_name_(node_name), name_space_(name_space), domain_id_(domain_id),
      is_active_(false) {
  // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
  std::string name = name_space.empty() ? name_space_ : name_space + "_";
  shm_prefix_ = "/" + std::to_string(domain_id_) + "_" + name;
}

Node::Node(const std::string &&node_name, const std::string &&name_space,
           int domain_id)
    : node_name_(node_name), name_space_(name_space), domain_id_(domain_id),
      is_active_(false) {
  // 初始化共享内存前缀（结合命名空间和域ID，避免冲突）
  std::string name = name_space.empty() ? name_space_ : name_space + "_";
  shm_prefix_ = "/" + std::to_string(domain_id_) + "_" + name;
}

bool Node::init() {
  std::lock_guard<std::mutex> lock(node_mutex_);
  if (is_active_) {
    return true; // 已初始化
  }
  // 这里可以添加更多初始化逻辑，如注册节点到某个管理器等
  is_active_ = true;
  return true;
}