#pragma once

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

// LOGD 宏：自动附加文件名和行号
// 使用流式输出，支持多个参数（用 << 连接）
#define LOGD(msg)                                                              \
  do {                                                                          \
    std::cout << "[" << getFileName(__FILE__) << ":" << __LINE__ << "] "       \
              << msg << std::endl;                                              \
  } while (0)

// LOGD_NO_NEWLINE：不自动换行的版本
#define LOGD_NO_NEWLINE(msg)                                                    \
  do {                                                                          \
    std::cout << "[" << getFileName(__FILE__) << ":" << __LINE__ << "] "       \
              << msg;                                                           \
  } while (0)

