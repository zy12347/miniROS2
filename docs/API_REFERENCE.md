# miniROS2 API 参考文档

本文档提供 miniROS2 框架的完整 API 参考。

## 目录

- [Node 类](#node-类)
- [Publisher 类](#publisher-类)
- [Subscriber 类](#subscriber-类)
- [Service 类](#service-类)
- [ClientRequest 类](#clientrequest-类)
- [QosPolicy 结构](#qospolicy-结构)
- [ShmBase 类](#shmbase-类)

## Node 类

### 构造函数

```cpp
Node(const std::string& node_name, 
     const std::string& name_space = "", 
     int domain_id = 0);
```

创建节点实例。

**参数：**
- `node_name`: 节点名称，必须唯一
- `name_space`: 命名空间，用于隔离不同应用的话题和服务
- `domain_id`: 域ID，用于多域隔离（默认0）

### 主要方法

#### createPublisher()

```cpp
template <typename MsgT>
std::shared_ptr<Publisher<MsgT>> createPublisher(
    const std::string& topic_name,
    QosPolicy qos_policy = QosPolicy()
);
```

创建发布者。

**参数：**
- `topic_name`: 话题名称
- `qos_policy`: QoS 策略（可选）

**返回：** 发布者智能指针

**示例：**
```cpp
auto pub = node.createPublisher<JsonValue>("chatter");
```

#### createSubscriber()

```cpp
template <typename MsgT>
std::shared_ptr<Subscriber<MsgT>> createSubscriber(
    const std::string& topic_name,
    const std::string& event_name,
    std::function<void(const MsgT&)> callback,
    QosPolicy qos_policy = QosPolicy()
);
```

创建订阅者。

**参数：**
- `topic_name`: 话题名称
- `event_name`: 事件名称
- `callback`: 消息回调函数
- `qos_policy`: QoS 策略（可选）

**返回：** 订阅者智能指针

**示例：**
```cpp
auto sub = node.createSubscriber<JsonValue>("chatter", "message",
    [](const JsonValue& msg) {
        std::cout << "Received: " << msg.serialize() << std::endl;
    });
```

#### spin()

```cpp
void spin();
```

启动事件循环（阻塞）。处理订阅消息、服务请求和定时器。

#### stop()

```cpp
void stop();
```

停止事件循环。

## Publisher 类

### publish()

```cpp
int publish(const std::string& event, const MsgT& data);
```

发布消息到指定事件。

**参数：**
- `event`: 事件名称
- `data`: 要发布的消息数据

**返回：** 0 表示成功，非0 表示失败

## Subscriber 类

订阅者通过回调函数接收消息，无需直接调用方法。

## QosPolicy 结构

### 枚举

```cpp
enum History {
    KEEP_LAST,  // 保留最后 N 条消息（环形缓冲区）
    KEEP_ALL    // 保留所有消息直到被读取（队列）
};
```

### 构造函数

```cpp
QosPolicy();  // 默认：KEEP_LAST, history_depth=1
QosPolicy(History history, int history_depth);
```

## 完整示例

参见 `examples/` 目录下的示例代码。

