# miniROS2 示例代码

本目录包含 miniROS2 框架的使用示例。

## 基本示例

### 1. 发布者示例 (talker_example.cpp)

演示如何创建节点和发布者，并发布消息。

```bash
cd build
cmake ..
make
./examples/talker_example
```

### 2. 订阅者示例 (listener_example.cpp)

演示如何创建节点和订阅者，并接收消息。

```bash
./examples/listener_example
```

### 3. 服务示例 (service_example.cpp)

演示如何创建服务端和客户端，实现请求-响应通信。

```bash
# 终端1：运行服务端
./examples/service_example

# 终端2：运行客户端
./examples/service_example client
```

## 高级示例

### 传感器示例

- `sensors/camera.cpp`: 相机节点示例
- `sensors/imu_node.cpp`: IMU 节点示例
- `sensors/lidar_node.cpp`: 激光雷达节点示例

### 处理节点示例

- `process/fusion_node.cpp`: 数据融合节点示例
- `process/laser_filter_node.cpp`: 激光滤波节点示例

### 共享内存示例

- `shared_memory_demo/shm_publisher.cpp`: 直接使用共享内存发布
- `shared_memory_demo/shm_subscriber.cpp`: 直接使用共享内存订阅

## 编译所有示例

```bash
cd build
cmake ..
make
```

所有示例程序将生成在 `build/examples/` 目录下。

