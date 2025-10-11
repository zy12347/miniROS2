#pragma once

#include "mini_ros2/message/json.h"
#include <cstring>
#include <stdexcept>
#include <type_traits>

class Serializer {
public:
  Serializer() {}
  template <typename T>
  static void serialize(const T &data, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(T) || buffer == nullptr) {
      throw std::runtime_error("Buffer size is too small or buffer is null");
    }
    memcpy(buffer, &data, sizeof(T));
  }
  template <typename T>
  static void deserialize(const uint8_t *buffer, size_t buffer_size, T &data) {
    if (buffer_size < sizeof(T) || buffer == nullptr) {
      throw std::runtime_error("Buffer size is too small or buffer is null");
    }
    memcpy(&data, buffer, sizeof(T));
  }

  template <typename T> static size_t getSerializedSize(const T &data) {
    return sizeof(T);
  }
};

template <>
inline void Serializer::serialize<JsonValue>(const JsonValue &data,
                                             uint8_t *buffer,
                                             size_t buffer_size) {
  if (buffer_size < data.serialize().size() || buffer == nullptr) {
    throw std::runtime_error("Buffer size is too small or buffer is null");
  }
  memcpy(buffer, data.serialize().c_str(), buffer_size);
}

template <>
inline void Serializer::deserialize<JsonValue>(const uint8_t *buffer,
                                               size_t buffer_size,
                                               JsonValue &data) {
  if (buffer_size < data.serialize().size() || buffer == nullptr) {
    throw std::runtime_error("Buffer size is too small or buffer is null");
  }
  data =
      JsonValue::deserialize(std::string(reinterpret_cast<const char *>(buffer),
                                         buffer_size)); // 包含终止符
}

template <>
inline size_t Serializer::getSerializedSize<JsonValue>(const JsonValue &data) {
  return data.serialize().size();
}
