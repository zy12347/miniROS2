// Timer 类：不持有 Node 引用，仅保存回调和周期
class Timer {
public:
  using Callback = std::function<void()>;
  using Ptr = std::shared_ptr<Timer>;

  Timer(std::chrono::milliseconds period, Callback callback)
      : period_(period), callback_(std::move(callback)),
        last_triggered_(std::chrono::steady_clock::now()), is_active_(true) {}

  // 检查是否到期（仅依赖自身状态）
  bool isReady() {
    if (!is_active_)
      return false;
    auto now = std::chrono::steady_clock::now();
    if (now - last_triggered_ >= period_) {
      last_triggered_ = now;
      return true;
    }
    return false;
  }

  // 执行回调（不访问 Node）
  void execute() {
    if (is_active_ && callback_) {
      callback_();
    }
  }

  // 停止定时器（由 Node 主动调用）
  void stop() { is_active_ = false; }

private:
  std::chrono::milliseconds period_;
  Callback callback_;
  std::chrono::steady_clock::time_point last_triggered_;
  std::atomic<bool> is_active_; // 控制定时器是否有效
};