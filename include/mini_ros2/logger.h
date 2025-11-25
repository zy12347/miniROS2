#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// 获取文件名（不含路径）
inline const char* getFileName(const char* file_path) {
  const char* last_slash = file_path;
  while (*file_path) {
    if (*file_path == '/' || *file_path == '\\') {
      last_slash = file_path + 1;
    }
    file_path++;
  }
  return last_slash;
}

// 获取精确到毫秒的当前本地时间字符串
inline std::string getCurrentTimeWithMs() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(&tm_buf, &t);
  std::tm* tm = &tm_buf;
#else
  std::tm* tm = localtime_r(&t, &tm_buf);
#endif
  std::ostringstream oss;
  if (tm) {
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
  }
  return oss.str();
}

// LOGD 宏：自动附加文件名和行号，精确到ms
#define LOGD(msg)                                                              \
  do {                                                                         \
    std::cout << "[" << getFileName(__FILE__) << ":" << __LINE__ << "] ";      \
    std::cout << getCurrentTimeWithMs() << " ";                                \
    std::cout << msg << std::endl;                                             \
  } while (0)

// LOGD_NO_NEWLINE：不自动换行的版本，精确到ms
#define LOGD_NO_NEWLINE(msg)                                                   \
  do {                                                                         \
    std::cout << "[" << getFileName(__FILE__) << ":" << __LINE__ << "] ";      \
    std::cout << getCurrentTimeWithMs() << " ";                                \
    std::cout << msg;                                                          \
  } while (0)

// LOGE 宏，精确到ms
#define LOGE(msg)                                                              \
  do {                                                                         \
    std::cerr << "[" << getFileName(__FILE__) << ":" << __LINE__               \
              << "] <ERROR> ";                                                 \
    std::cerr << getCurrentTimeWithMs() << " ";                                \
    std::cerr << msg << std::endl;                                             \
    std::cerr.flush();                                                         \
  } while (0)