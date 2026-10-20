#pragma once
/** @file MetricsTypes.h
 * @brief szx_metrics 的稳定 C ABI 数据契约。
 *
 * @details 跨库接口仅使用 C 基本类型和不透明句柄，不传递 STL 对象。
 *          可扩展出参以 struct_size 开头：调用方写入自身缓冲区大小，库在
 *          大小不低于该类型支持的最小前缀时，最多写入库内已知的结构体大小。
 *          调用方缓冲区超出库内布局的部分不会被修改；嵌套结构体由库填写大小。
 *          快照和历史结果中的 UTF-8 字符串由所属对象持有，释放对象后失效；
 *          服务运行标识的返回指针另见 Metrics.h 中该函数的生命周期说明。
 *          函数以 int32_t 错误码报告失败，0 表示成功。
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** ABI 版本号：接口不兼容变更时递增，调用方可据此拒绝加载。 */
#define SZX_METRICS_ABI_VERSION 1u

/** 明确类型的大小，供调用方填充 struct_size。 */
#define SZX_METRICS_SIZE_OF(type) ((uint32_t)sizeof(type))

/** 不透明句柄：定义在库内部，调用方只持有指针。 */
typedef struct szx_metrics_service szx_metrics_service;
typedef struct szx_metrics_snapshot szx_metrics_snapshot;
typedef struct szx_metrics_history szx_metrics_history;

/** 通用返回码。 */
enum szx_metrics_result
{
    SZX_METRICS_OK               = 0, /**< 成功 */
    SZX_METRICS_INVALID_ARGUMENT = 1, /**< 空指针、struct_size 过小等 */
    SZX_METRICS_INVALID_CONFIG   = 2, /**< 配置越界（路径非法、数量超限） */
    SZX_METRICS_OUT_OF_MEMORY    = 3, /**< 分配失败 */
    SZX_METRICS_NOT_RUNNING      = 4, /**< 服务未启动或还没有样本 */
    SZX_METRICS_OUT_OF_RANGE     = 5, /**< 下标 / 窗口越界 */
    SZX_METRICS_PLATFORM_ERROR   = 6, /**< 操作系统采集接口失败 */
    SZX_METRICS_INTERNAL_ERROR   = 7  /**< 其余内部错误 */
};

/** 单项指标状态。与错误码分离：状态描述「这一项采到了没有」。 */
enum szx_metrics_metric_state
{
    SZX_METRICS_METRIC_UNAVAILABLE = 0, /**< 平台不支持或采集失败 */
    SZX_METRICS_METRIC_WARMING_UP  = 1, /**< 差分型指标首帧，尚无可用值 */
    SZX_METRICS_METRIC_OK          = 2, /**< 有有效数据 */
    SZX_METRICS_METRIC_STOPPED     = 3  /**< 被监控的进程不存在 */
};

/** szx_metrics_metric::present 位掩码：该字段是否有意义。 */
enum szx_metrics_metric_field
{
    SZX_METRICS_FIELD_USAGE       = 1u << 0, /**< usage_percent */
    SZX_METRICS_FIELD_TEMPERATURE = 1u << 1, /**< temperature_celsius */
    SZX_METRICS_FIELD_TOTAL       = 1u << 2, /**< total_bytes */
    SZX_METRICS_FIELD_USED        = 1u << 3, /**< used_bytes */
    SZX_METRICS_FIELD_AVAILABLE   = 1u << 4, /**< available_bytes */
    SZX_METRICS_FIELD_FREE        = 1u << 5  /**< free_bytes */
};

/** 卡片类别开关位（szx_metrics_config::flags / service_set_cards）。 */
enum szx_metrics_card
{
    SZX_METRICS_CARD_CPU     = 1u << 0,
    SZX_METRICS_CARD_MEMORY  = 1u << 1,
    SZX_METRICS_CARD_DISK    = 1u << 2,
    SZX_METRICS_CARD_NETWORK = 1u << 3,
    SZX_METRICS_CARD_PROCESS = 1u << 4,
    SZX_METRICS_CARD_GPU     = 1u << 5,
    SZX_METRICS_CARD_ALL     = 0x3Fu
};

/** 一项指标。
 * @details state 表示采集状态，present 表示哪些值有效。内存的 available
 *          包含可回收缓存，不等于原始空闲，因此通常不设置 FREE 位。
 *          v1 接受 32 字节的旧版前缀；新调用方应填写完整 sizeof。
 */
typedef struct szx_metrics_metric
{
    uint32_t struct_size;       /**< [in] 填 SZX_METRICS_SIZE_OF(szx_metrics_metric) */
    uint32_t state;             /**< [out] szx_metrics_metric_state */
    uint32_t present;           /**< [out] szx_metrics_metric_field 位掩码 */
    uint32_t reserved;          /**< [out] 保留，恒为 0 */
    double usage_percent;       /**< [out] 0~100，SZX_METRICS_FIELD_USAGE 置位时有效 */
    double temperature_celsius; /**< [out] 摄氏度，SZX_METRICS_FIELD_TEMPERATURE 置位时有效 */
    uint64_t total_bytes;       /**< [out] 总容量字节数，SZX_METRICS_FIELD_TOTAL 置位时有效 */
    uint64_t used_bytes;        /**< [out] 已用字节数，SZX_METRICS_FIELD_USED 置位时有效 */
    uint64_t available_bytes;   /**< [out] 可用字节数，SZX_METRICS_FIELD_AVAILABLE 置位时有效 */
    uint64_t free_bytes;        /**< [out] 原始空闲字节数，SZX_METRICS_FIELD_FREE 置位时有效 */
} szx_metrics_metric;

/** 一个本地卷。 */
typedef struct szx_metrics_disk
{
    uint32_t struct_size;        /**< [in] 填 SZX_METRICS_SIZE_OF(szx_metrics_disk) */
    uint32_t reserved;           /**< [out] */
    const char *id;              /**< [out] 卷标识；同一卷的多个挂载点只出现一次 */
    const char *path;            /**< [out] 卷根路径，UTF-8 */
    const char *type;            /**< [out] 介质/总线描述，如 "SSD(NVMe)"；不可用时为平台占位描述 */
    int64_t sampled_at;          /**< [out] 采样时间，Unix 毫秒 */
    szx_metrics_metric capacity; /**< [out] 容量指标，嵌套 struct_size 由库填写 */
} szx_metrics_disk;

/** 一个网口。 */
typedef struct szx_metrics_network
{
    uint32_t struct_size;    /**< [in] */
    uint32_t reserved;       /**< [out] */
    const char *id;          /**< [out] 平台稳定标识 */
    const char *name;        /**< [out] 连接名，如 "以太网 3" */
    const char *description; /**< [out] 设备型号 */
    const char *ipv4;        /**< [out] 单播 IPv4，多个以 '\n' 分隔；无地址时为 "" */
    const char *ipv6;        /**< [out] 单播 IPv6，多个以 '\n' 分隔；无地址时为 "" */
    int64_t sampled_at;      /**< [out] Unix 毫秒 */
    double rx_bytes_per_sec; /**< [out] 下载速率；未连接时为 0，未知时 present=0 */
    double tx_bytes_per_sec; /**< [out] 上传速率；同上 */
    double link_mbps;        /**< [out] 协商带宽 Mbps；未知时 present=0 */
    uint32_t present;        /**< [out] 位 0/1/2 = rx/tx/link 有效 */
    uint32_t reserved2;      /**< [out] */
} szx_metrics_network;

/** 一个被关注进程的占用。 */
typedef struct szx_metrics_process
{
    uint32_t struct_size;  /**< [in] */
    uint32_t state;        /**< [out] szx_metrics_metric_state：OK 或 STOPPED */
    const char *name;      /**< [out] 调用方配置的进程名（小写，不带扩展名） */
    double cpu_percent;    /**< [out] 全核归一后的 0~100 */
    uint64_t memory_bytes; /**< [out] 工作集字节 */
    uint32_t reserved;     /**< [out] */
} szx_metrics_process;

/** 显卡补充用量。 */
typedef struct szx_metrics_gpu_extra
{
    uint32_t struct_size;       /**< [in] */
    uint32_t state;             /**< [out] szx_metrics_metric_state */
    double total_percent;       /**< [out] 全引擎占用率（任务管理器口径） */
    double cuda_percent;        /**< [out] CUDA 引擎占用率 */
    double graphics_3d_percent; /**< [out] 3D 引擎占用率 */
    uint32_t present;           /**< [out] 位 0/1/2 = total/cuda/3d 有效 */
    uint32_t reserved;          /**< [out] */
} szx_metrics_gpu_extra;

/** 一项按程序名归类的数值（显存 bytes 或引擎占用率）。 */
typedef struct szx_metrics_named_value
{
    uint32_t struct_size; /**< [in] */
    const char *name;     /**< [out] 程序名，如 "chrome" */
    double value;         /**< [out] 字节数或百分比，含义由调用方查询的函数决定 */
} szx_metrics_named_value;

/** 历史采样点。只保留画曲线所需的字段。 */
typedef struct szx_metrics_sample
{
    uint32_t struct_size;      /**< [in] */
    uint32_t reserved;         /**< [out] */
    uint64_t sequence;         /**< [out] 从 1 开始，服务重启后重新计数 */
    int64_t sampled_at;        /**< [out] Unix 毫秒 */
    szx_metrics_metric cpu;    /**< [in/out] struct_size 由库填写 */
    szx_metrics_metric memory; /**< [in/out] */
    szx_metrics_metric gpu;    /**< [in/out] */
} szx_metrics_sample;

/** 服务创建参数。
 * @details 两个周期字段为 0 时分别使用 2000/30000 毫秒，非零值限制在
 *          [100, 60000] 毫秒。history_capacity 为 0 时使用 450 帧，非零值
 *          限制在 [10, 7200] 帧。路径和进程名在创建时复制；state_path 非空
 *          时，启动会尝试恢复已保存的卡片开关，覆盖 flags。
 */
typedef struct szx_metrics_config
{
    uint32_t struct_size;             /**< [in] 必须最先填写 */
    uint32_t flags;                   /**< [in] szx_metrics_card 位掩码；0 表示全部关闭，config_init 默认全部开启 */
    const char *const *disk_paths;    /**< [in] 只监控这些绝对路径；NULL/空 表示自动发现 */
    uint32_t disk_path_count;         /**< [in] 最多 32 个 */
    const char *const *process_names; /**< [in] 关注进程名（不带 .exe） */
    uint32_t process_name_count;      /**< [in] 最多 16 个 */
    const char *state_path;           /**< [in] 卡片开关持久化文件；NULL/空 表示不持久化 */
    uint32_t sample_interval_ms;      /**< [in] 0 = 2000 */
    uint32_t disk_interval_ms;        /**< [in] 0 = 30000 */
    uint32_t history_capacity;        /**< [in] 0 = 450 帧 */
} szx_metrics_config;

/** 固定上限，供调用方在配置时就地校验。 */
#define SZX_METRICS_MAX_DISK_PATHS 32u
#define SZX_METRICS_MAX_PROCESS_NAMES 16u
#define SZX_METRICS_MAX_WINDOW_SECONDS 900u
#define SZX_METRICS_DEFAULT_SAMPLE_INTERVAL_MS 2000u
#define SZX_METRICS_DEFAULT_DISK_INTERVAL_MS 30000u
#define SZX_METRICS_DEFAULT_HISTORY_CAPACITY 450u

#ifdef __cplusplus
} /* extern "C" */
#endif
