#pragma once
/** @file Metrics.h
 * @brief 系统指标采集服务的稳定 C ABI。
 *
 * @details 设计约束（与 MetricsTypes.h 配套阅读）：
 *          - 接口平台无关：本头文件不出现 windows.h / unistd.h，Windows 与 Linux
 *            编译出完全相同的符号表，调用方代码无需条件编译；
 *          - 接口稳定：只使用 C 基本类型、POD 结构体与不透明句柄，不跨边界传递
 *            STL 对象或异常；结构体带 struct_size 支持前后向兼容；
 *          - 同一份头文件支持动态库、静态库、直接编译源码（见 MetricsExport.h）；
 *          - 服务对象由调用方管理生命周期；快照和历史是独立分配的只读结果，
 *            调用方必须分别调用对应的 release，结果中的字符串只在结果存活期间有效；
 *          - 销毁服务前，应先结束所有对该服务句柄的并发调用。
 *
 * 典型用法：
 * @code
 * szx_metrics_config config;
 * szx_metrics_config_init(&config);          // 必须先初始化 struct_size
 * szx_metrics_service* service = nullptr;
 * if (szx_metrics_service_create(&config, &service) != SZX_METRICS_OK) { ... }
 * if (szx_metrics_service_start(service) != SZX_METRICS_OK) { ... }
 *
 * szx_metrics_snapshot* snapshot = nullptr;
 * if (szx_metrics_service_snapshot(service, &snapshot) == SZX_METRICS_OK) {
 *     szx_metrics_metric cpu;
 *     cpu.struct_size = SZX_METRICS_SIZE_OF(szx_metrics_metric);
 *     szx_metrics_snapshot_cpu(snapshot, &cpu);
 *     // instance 在 snapshot_release 之前有效
 *     const char* instance = szx_metrics_snapshot_instance_id(snapshot);
 *     szx_metrics_snapshot_release(snapshot);
 * }
 * szx_metrics_service_destroy(service);
 * @endcode
 */

#include "MetricsExport.h"
#include "MetricsTypes.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* 版本与配置                                                                 */
/* ------------------------------------------------------------------------- */

/** 返回库编译时的 ABI 版本，应与 SZX_METRICS_ABI_VERSION 相等。 */
SZX_METRICS_API uint32_t szx_metrics_abi_version(void);

/** 初始化配置为默认值：全部卡片开启、自动发现磁盘、无关注进程、不持久化。 */
SZX_METRICS_API void szx_metrics_config_init(szx_metrics_config *config);

/* ------------------------------------------------------------------------- */
/* 服务生命周期                                                               */
/* ------------------------------------------------------------------------- */

/** 创建服务；配置中的路径和进程名会被复制，返回后调用方可释放原缓冲区。
 * @param[in] config 配置；NULL 使用 config_init 的默认值。
 * @param[out] out 非空；成功时写入新对象，失败时写入 NULL。
 * @return SZX_METRICS_OK / SZX_METRICS_INVALID_ARGUMENT / SZX_METRICS_INVALID_CONFIG /
 *         SZX_METRICS_OUT_OF_MEMORY / SZX_METRICS_INTERNAL_ERROR
 */
SZX_METRICS_API int32_t szx_metrics_service_create(const szx_metrics_config *config, szx_metrics_service **out);

/** 停止采样并销毁服务；传入 NULL 无操作。调用方须先结束并发服务调用。 */
SZX_METRICS_API void szx_metrics_service_destroy(szx_metrics_service *service);

/** 启动采样线程并立即产出首帧（首帧 CPU 为 WARMING_UP，内存已可用）。
 * @details 重复调用返回 SZX_METRICS_INVALID_CONFIG；可在 stop() 后再次启动，
 *          但 instance_id 会更换，历史样本清空。
 */
SZX_METRICS_API int32_t szx_metrics_service_start(szx_metrics_service *service);

/** 停止并回收采样线程；可重复调用，可在服务销毁前任意线程调用。 */
SZX_METRICS_API void szx_metrics_service_stop(szx_metrics_service *service);

/** 采集线程是否在运行。 */
SZX_METRICS_API int32_t szx_metrics_service_is_running(const szx_metrics_service *service);

/** 本次运行标识；未启动时为空串。
 * @note 指针由调用线程的缓存持有，同线程下次调用本函数时内容可能被替换。
 */
SZX_METRICS_API const char *szx_metrics_service_instance_id(const szx_metrics_service *service);

/* ------------------------------------------------------------------------- */
/* 快照                                                                       */
/* ------------------------------------------------------------------------- */

/** 取最近一帧；未启动或尚无样本时返回 SZX_METRICS_NOT_RUNNING。
 * @param[out] out 非空；成功时写入需由 snapshot_release 释放的对象，失败时写入 NULL。
 */
SZX_METRICS_API int32_t szx_metrics_service_snapshot(const szx_metrics_service *service, szx_metrics_snapshot **out);

/** 释放快照。传入 NULL 是安全的空操作。 */
SZX_METRICS_API void szx_metrics_snapshot_release(szx_metrics_snapshot *snapshot);

/** 快照序号，从 1 开始；NULL 返回 0。 */
SZX_METRICS_API uint64_t szx_metrics_snapshot_sequence(const szx_metrics_snapshot *snapshot);
/** 采样时间，Unix 毫秒；NULL 返回 0。 */
SZX_METRICS_API int64_t szx_metrics_snapshot_sampled_at(const szx_metrics_snapshot *snapshot);
/** 快照所属运行标识；NULL 返回空串。字符串有效至释放快照。 */
SZX_METRICS_API const char *szx_metrics_snapshot_instance_id(const szx_metrics_snapshot *snapshot);
/** 平台标识："windows" / "linux" / "unknown"；NULL 返回 "unknown"。 */
SZX_METRICS_API const char *szx_metrics_snapshot_platform(const szx_metrics_snapshot *snapshot);
/** CPU 型号；无数据或 NULL 返回空串。 */
SZX_METRICS_API const char *szx_metrics_snapshot_cpu_name(const szx_metrics_snapshot *snapshot);
/** 内存规格描述；无数据或 NULL 返回空串。 */
SZX_METRICS_API const char *szx_metrics_snapshot_memory_spec(const szx_metrics_snapshot *snapshot);
/** 逻辑处理器数；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_logical_cpus(const szx_metrics_snapshot *snapshot);
/** CPU、内存等指标的配置采样周期，毫秒；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_sample_interval_ms(const szx_metrics_snapshot *snapshot);
/** 磁盘容量的配置采样周期，毫秒；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_disk_interval_ms(const szx_metrics_snapshot *snapshot);

/** 读取 CPU 指标；out 先填 struct_size，指标是否可用由 state/present 表示。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_cpu(const szx_metrics_snapshot *snapshot, szx_metrics_metric *out);
/** 读取内存指标；available 包含可回收缓存，FREE 位通常不置位。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_memory(const szx_metrics_snapshot *snapshot, szx_metrics_metric *out);
/** 读取第一块 GPU 的指标；没有 GPU 时仍可成功返回 UNAVAILABLE 状态。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_gpu(const szx_metrics_snapshot *snapshot, szx_metrics_metric *out);
/** 读取第一块 GPU 的引擎占用；各字段有效性见 present，out 先填 struct_size。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_gpu_extra(const szx_metrics_snapshot *snapshot, szx_metrics_gpu_extra *out);
/** 检测到的 GPU 数；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_gpu_count(const szx_metrics_snapshot *snapshot);
/** 第一块 GPU 的名称；无数据或 NULL 返回空串。 */
SZX_METRICS_API const char *szx_metrics_snapshot_gpu_name(const szx_metrics_snapshot *snapshot);
/** GPU 显存与引擎占用涉及的程序名并集行数；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_gpu_process_count(const szx_metrics_snapshot *snapshot);
/** 按程序名排序读取 GPU 明细；优先返回显存字节数，无显存记录时返回占用率。
 * @param[in] index 从 0 开始，越界返回 SZX_METRICS_OUT_OF_RANGE。
 * @param[in,out] out 先填 struct_size；name 的有效期与快照一致。
 */
SZX_METRICS_API int32_t szx_metrics_snapshot_gpu_process_at(const szx_metrics_snapshot *snapshot, uint32_t index,
                                                            szx_metrics_named_value *out);
/** 当前启用的卡片位掩码（szx_metrics_card）。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_cards(const szx_metrics_snapshot *snapshot);

/** 快照中的本地卷数量，不超过 SZX_METRICS_MAX_DISK_PATHS；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_disk_count(const szx_metrics_snapshot *snapshot);
/** 读取第 index 个本地卷；out 先填 struct_size，index 越界返回 OUT_OF_RANGE。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_disk_at(const szx_metrics_snapshot *snapshot, uint32_t index, szx_metrics_disk *out);

/** 快照中的网口数量；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_network_count(const szx_metrics_snapshot *snapshot);
/** 读取第 index 个网口；out 先填 struct_size，字符串有效期与快照一致。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_network_at(const szx_metrics_snapshot *snapshot, uint32_t index,
                                                        szx_metrics_network *out);

/** 快照中的关注进程条目数；同名进程合并，NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_snapshot_process_count(const szx_metrics_snapshot *snapshot);
/** 读取第 index 个进程条目；out 先填 struct_size，未运行的进程状态为 STOPPED。 */
SZX_METRICS_API int32_t szx_metrics_snapshot_process_at(const szx_metrics_snapshot *snapshot, uint32_t index,
                                                        szx_metrics_process *out);

/* ------------------------------------------------------------------------- */
/* 历史                                                                       */
/* ------------------------------------------------------------------------- */

/** 取窗口内的采样序列；window_seconds 取值 (0, 900]。
 * @param[out] out 非空；成功时写入需由 history_release 释放的对象，失败时写入 NULL。
 * @return SZX_METRICS_OK / SZX_METRICS_INVALID_ARGUMENT / SZX_METRICS_OUT_OF_RANGE /
 *         SZX_METRICS_NOT_RUNNING / SZX_METRICS_OUT_OF_MEMORY
 */
SZX_METRICS_API int32_t szx_metrics_service_history(const szx_metrics_service *service, uint32_t window_seconds,
                                                    szx_metrics_history **out);

/** 释放历史结果及其字符串；NULL 无操作。 */
SZX_METRICS_API void szx_metrics_history_release(szx_metrics_history *history);

/** 历史结果所属运行标识；字符串有效至释放历史结果，NULL 返回空串。 */
SZX_METRICS_API const char *szx_metrics_history_instance_id(const szx_metrics_history *history);
/** 历史结果的采样周期，毫秒；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_history_sample_interval_ms(const szx_metrics_history *history);
/** 查询使用的时间窗口，秒；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_history_window_seconds(const szx_metrics_history *history);
/** 实际样本数，受时间窗口与历史容量共同限制；NULL 返回 0。 */
SZX_METRICS_API uint32_t szx_metrics_history_count(const szx_metrics_history *history);
/** 按时间升序读取样本；out 先填 struct_size，index 越界返回 OUT_OF_RANGE。 */
SZX_METRICS_API int32_t szx_metrics_history_sample_at(const szx_metrics_history *history, uint32_t index,
                                                      szx_metrics_sample *out);

/* ------------------------------------------------------------------------- */
/* 卡片开关                                                                   */
/* ------------------------------------------------------------------------- */

/** 当前开关。服务为 NULL 时返回 SZX_METRICS_CARD_ALL。 */
SZX_METRICS_API uint32_t szx_metrics_service_cards(const szx_metrics_service *service);

/** 更新开关；采集线程下次读取开关后生效，state_path 非空时尝试持久化。
 * @param[in] flags szx_metrics_card 位掩码；未知位被忽略，0 关闭全部卡片。
 * @note 持久化失败不影响采集，也不会使本函数返回失败。
 */
SZX_METRICS_API int32_t szx_metrics_service_set_cards(szx_metrics_service *service, uint32_t flags);

/* ------------------------------------------------------------------------- */
/* 纯计算 / 解析助手：与平台无关、无副作用，便于调用方做校验与单元测试          */
/* ------------------------------------------------------------------------- */

/** CPU 累计时钟计数，用于相邻采样差分。 */
typedef struct szx_metrics_cpu_counters
{
    uint32_t struct_size; /**< 结构体大小；解析出参先填 SZX_METRICS_SIZE_OF。 */
    uint32_t reserved;    /**< 保留，置 0。 */
    uint64_t total;       /**< user/nice/system/idle/iowait/irq/softirq/steal 总和。 */
    uint64_t idle;        /**< idle 与 iowait 总和；单位与 total 一致。 */
} szx_metrics_cpu_counters;

/** 解析 /proc/stat 首行（"cpu  user nice system idle iowait irq softirq steal ..."）。
 * @param text UTF-8 文本；length 可为 0 表示以 NUL 结尾。
 * @return 成功时填充 out；首行不是 cpu 汇总行、字段不足或计数溢出时返回
 *         SZX_METRICS_INVALID_ARGUMENT。
 */
SZX_METRICS_API int32_t szx_metrics_parse_cpu_counters(const char *text, uint32_t length, szx_metrics_cpu_counters *out);

/** 由两次计数差算 CPU 使用率。
 * @return 可计算时返回 SZX_METRICS_OK 并写 out_percent；计数回退、无增量或
 *         idle 越界时返回 SZX_METRICS_OUT_OF_RANGE。
 */
SZX_METRICS_API int32_t szx_metrics_cpu_usage(const szx_metrics_cpu_counters *previous, const szx_metrics_cpu_counters *current,
                                              double *out_percent);

/** 解析 /proc/meminfo 的 MemTotal / MemAvailable，kB 转换为字节。
 * @note 输入缺失或非法时返回 OK，并将 out->state 设为 UNAVAILABLE；需先填 struct_size。
 */
SZX_METRICS_API int32_t szx_metrics_parse_memory(const char *text, uint32_t length, szx_metrics_metric *out);

/** 由容量字节数算指标；total 为 0 或不满足 available <= free <= total 时状态为 UNAVAILABLE。
 * @note 容量无效与出参非法不同；前者返回 OK，后者返回 INVALID_ARGUMENT。
 */
SZX_METRICS_API int32_t szx_metrics_capacity_metric(uint64_t total, uint64_t free, uint64_t available, szx_metrics_metric *out);

#ifdef __cplusplus
} /* extern "C" */
#endif
