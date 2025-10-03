// // QoS配置结构体（精简核心参数）
// typedef struct {
//   // 可靠性：BEST_EFFORT（允许丢失）/ RELIABLE（确保送达）
//   enum Reliability { BEST_EFFORT, RELIABLE } reliability;

//   // 历史记录：KEEP_LAST（保留最后N个）/ KEEP_ALL（保留所有，受资源限制）
//   enum History { KEEP_LAST, KEEP_ALL } history;
//   uint32_t history_depth; // KEEP_LAST时的缓存深度

//   // 持续性：VOLATILE（发布者退出后数据失效）/
//   TRANSIENT_LOCAL（保留在共享内存） enum Durability { VOLATILE,
//   TRANSIENT_LOCAL } durability;

//   // 资源限制：共享内存中最大缓存样本数/总大小
//   uint32_t max_samples;   // 最大样本数
//   size_t max_sample_size; // 单样本最大大小（字节）

//   // 可选：截止时间（毫秒，适用于实时场景）
//   uint32_t deadline_ms;
// } QoSProfile;

// // 共享内存中的主题数据缓冲区
// typedef struct {
//   char *buffer;       // 共享内存地址
//   size_t sample_size; // 单样本大小
//   uint32_t capacity;  // 最大样本数（= history_depth 或 max_samples）
//   uint32_t write_idx; // 当前写入位置（环形缓冲区索引）
//   uint32_t read_idx; // 订阅者已读位置（每个订阅者维护独立索引）
//   pthread_mutex_t mutex; // 同步锁（保护缓冲区读写）
// } SharedMemoryBuffer;

// // 带确认标记的样本（扩展缓冲区元素）
// typedef struct {
//   void *data;         // 实际数据
//   bool is_confirmed;  // 是否被订阅者确认
//   uint64_t timestamp; // 时间戳（用于超时判断）
// } ReliableSample;

// // 持续性数据管理结构
// typedef struct {
//   char *data;           // 数据内容
//   uint32_t ref_count;   // 引用计数（订阅者数量）
//   bool is_valid;        // 数据是否有效
//   pthread_mutex_t lock; // 保护引用计数
// } TransientData;

// // 共享内存池管理
// typedef struct {
//   char *pool;           // 共享内存池起始地址
//   size_t total_size;    // 总大小
//   size_t used_size;     // 已使用大小
//   pthread_mutex_t lock; // 保护内存分配
// } SharedMemoryPool;