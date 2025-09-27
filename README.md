classDiagram
    direction LR
    
    class SharedMemory {
        - string name_
        - size_t size_
        - int fd_
        - void* data_
        + SharedMemory(string, size_t)
        + create() bool
        + open() bool
        + data() void*
        + unlink(string) bool
    }
    
    class Semaphore {
        - string name_
        - sem_t* sem_
        + Semaphore(string, unsigned int)
        + wait() void
        + post() void
        + unlink(string) bool
    }
    
    class QoSPolicy {
        <<enumeration>> ReliabilityPolicy
        <<enumeration>> HistoryPolicy
        - ReliabilityPolicy reliability
        - HistoryPolicy history
        - uint32_t history_depth
        - milliseconds deadline
        - size_t max_message_size
        - size_t max_total_size
    }
    
    class QoSMessageBuffer~T~ {
        - string name_
        - QoSPolicy qos_
        - unique_ptr~SharedMemory~ shm_
        - TimedMessage~T~* messages_
        - size_t write_idx_
        - size_t read_idx_
        + write(T) void
        + readLatest(T) bool
        + readHistory(size_t) vector~T~
    }
    
    class NodeDiscovery {
        - string node_name_
        - unique_ptr~SharedMemory~ registry_shm_
        - NodeRegistry* registry_
        + NodeDiscovery(string)
        + registerNode() void
        + registerTopic(string, string, bool) void
        + findPublishers(string) vector~pid_t~
    }
    
    class Node {
        - string name_
        - NodeDiscovery discovery_
        - pid_t pid_
        + Node(string)
        + getName() string
    }
    
    class Publisher~T~ {
        - Node* node_
        - string topic_name_
        - QoSPolicy qos_
        - unique_ptr~QoSMessageBuffer~ buffer_
        - unique_ptr~Semaphore~ sem_
        - thread deadline_thread_
        + Publisher(Node*, string, QoSPolicy)
        + publish(T) void
        + setDeadlineCallback(function) void
    }
    
    class Subscriber~T~ {
        - Node* node_
        - string topic_name_
        - QoSPolicy qos_
        - unique_ptr~QoSMessageBuffer~ buffer_
        - unique_ptr~Semaphore~ sem_
        - thread thread_
        - function callback_
        - bool running_
        + Subscriber(Node*, string, function, QoSPolicy)
        + ~Subscriber()
    }
    
    class Service~Req,Res~ {
        - Node* node_
        - string service_name_
        - QoSPolicy qos_
        - function callback_
        - SharedMemory shm_req_
        - SharedMemory shm_res_
        - Semaphore sem_req_
        - Semaphore sem_res_
        + Service(Node*, string, function, QoSPolicy)
        + start() void
    }
    
    class Client~Req,Res~ {
        - Node* node_
        - string service_name_
        - QoSPolicy qos_
        - SharedMemory shm_req_
        - SharedMemory shm_res_
        - Semaphore sem_req_
        - Semaphore sem_res_
        + Client(Node*, string, QoSPolicy)
        + call(Req, Res) bool
    }
    
    QoSMessageBuffer --|> SharedMemory : uses
    QoSMessageBuffer --|> QoSPolicy : uses
    
    Publisher --|> Node : has
    Publisher --|> QoSMessageBuffer : uses
    Publisher --|> Semaphore : uses
    Publisher --|> QoSPolicy : uses
    
    Subscriber --|> Node : has
    Subscriber --|> QoSMessageBuffer : uses
    Subscriber --|> Semaphore : uses
    Subscriber --|> QoSPolicy : uses
    
    Node --|> NodeDiscovery : contains
    
    NodeDiscovery --|> SharedMemory : uses
    NodeDiscovery --|> Semaphore : uses
    
    Service --|> Node : has
    Service --|> SharedMemory : uses
    Service --|> Semaphore : uses
    Service --|> QoSPolicy : uses
    
    Client --|> Node : has
    Client --|> SharedMemory : uses
    Client --|> Semaphore : uses
    Client --|> QoSPolicy : uses
    
    Service -- Client : communicates with
    Publisher -- Subscriber : communicates with
