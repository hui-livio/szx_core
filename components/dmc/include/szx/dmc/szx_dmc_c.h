#pragma once

/**
 * @file szx_dmc_c.h
 * @brief SZX DMC 的版本化 C ABI。
 *
 * 所有结构体都带有结构体大小和 ABI 版本，调用方应在访问字段前校验兼容性。
 * 所有导出函数均不会抛出 C++ 异常。
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
    #if defined(SZX_DMC_C_EXPORTS) || defined(SZX_DMC_C_EXPORT)
        #define SZX_DMC_C_API __declspec(dllexport)
    #else
        #define SZX_DMC_C_API __declspec(dllimport)
    #endif
    #define SZX_DMC_CALL __cdecl
#else
    #define SZX_DMC_C_API __attribute__((visibility("default")))
    #define SZX_DMC_CALL
#endif

#ifdef __cplusplus
extern "C" {
    #define SZX_DMC_NOEXCEPT noexcept
#else
    #define SZX_DMC_NOEXCEPT
#endif

/** @brief 不透明的 DMC 控制器句柄。 */
typedef struct szx_dmc_handle szx_dmc_handle;

/** @brief 日志回调级别。 */
typedef enum szx_dmc_log_level_v1
{
    SZX_DMC_LOG_DEBUG   = 0, /**< 调试信息。 */
    SZX_DMC_LOG_TRACE   = 1, /**< 运行轨迹信息。 */
    SZX_DMC_LOG_INFO    = 2, /**< 常规运行信息。 */
    SZX_DMC_LOG_WARNING = 3, /**< 警告信息。 */
    SZX_DMC_LOG_ERROR   = 4, /**< 错误信息。 */
} szx_dmc_log_level_v1;

/**
 * @brief DMC 日志回调函数类型。
 * @param[in] level 日志级别，对应 szx_dmc_log_level_v1。
 * @param[in] message 以零结尾的日志文本；仅在回调执行期间有效。
 * @param[in] user_data 创建配置中传入的用户数据指针。
 */
typedef void(SZX_DMC_CALL *szx_dmc_log_callback_v1)(uint8_t level, const char *message, void *user_data);

enum
{
    SZX_DMC_ABI_VERSION_V1         = 1,   /**< 当前 C ABI 版本。 */
    SZX_DMC_DIAGNOSTIC_CAPACITY_V1 = 128, /**< 状态诊断文本缓冲区容量。 */
};

/** @brief C ABI 返回的稳定错误码。 */
typedef enum szx_dmc_error_v1
{
    SZX_DMC_SUCCESS                = 0,  /**< 操作成功。 */
    SZX_DMC_INVALID_ARGUMENT       = 1,  /**< 参数或配置无效。 */
    SZX_DMC_DRIVER_UNAVAILABLE     = 2,  /**< 驱动不可用。 */
    SZX_DMC_ARCHITECTURE_MISMATCH  = 3,  /**< 驱动与进程架构不匹配。 */
    SZX_DMC_SYMBOL_UNAVAILABLE     = 4,  /**< 驱动缺少所需函数符号。 */
    SZX_DMC_BOARD_NOT_FOUND        = 5,  /**< 找不到指定板卡。 */
    SZX_DMC_NOT_CONNECTED          = 6,  /**< 控制器尚未连接。 */
    SZX_DMC_BUSY                   = 7,  /**< 控制器正忙。 */
    SZX_DMC_SAFETY_LATCHED         = 8,  /**< 安全停止已锁存。 */
    SZX_DMC_AXIS_NOT_READY         = 9,  /**< 轴尚未就绪。 */
    SZX_DMC_NOT_HOMED              = 10, /**< 轴尚未回零。 */
    SZX_DMC_LIMIT_REACHED          = 11, /**< 已到达运动限位。 */
    SZX_DMC_MOTION_TIMEOUT         = 12, /**< 运动超时。 */
    SZX_DMC_IO_UNAVAILABLE         = 13, /**< 输入或输出不可用。 */
    SZX_DMC_COMPARE_UNAVAILABLE    = 14, /**< 位置比较功能不可用。 */
    SZX_DMC_COMPARE_QUEUE_FULL     = 15, /**< 位置比较队列已满。 */
    SZX_DMC_TRIGGER_TIMEOUT        = 16, /**< 等待触发超时。 */
    SZX_DMC_FRAME_PROGRESS_INVALID = 17, /**< 相机帧进度无效。 */
    SZX_DMC_CANCELLED              = 18, /**< 任务已取消。 */
    SZX_DMC_RESOURCE_EXHAUSTED     = 19, /**< 可用资源不足。 */
    SZX_DMC_UNEXPECTED_FAILURE     = 20, /**< 发生未预期的实现错误。 */
    SZX_DMC_BUFFER_TOO_SMALL       = 21, /**< 调用方输出缓冲区太小。 */
} szx_dmc_error_v1;

/** @brief 工厂支持的 DMC 驱动系列。 */
typedef enum szx_dmc_model_v1
{
    SZX_DMC_MODEL_LEISAI_LTDMC  = 0,                          /**< 雷赛 LTDMC 驱动系列。 */
    SZX_DMC_MODEL_CUSTOM        = 255,                        /**< 自定义控制器系列，当前未注册实现。 */
    SZX_DMC_MODEL_GENERIC_LTDMC = SZX_DMC_MODEL_LEISAI_LTDMC, /**< 雷赛 LTDMC 驱动系列。 */
} szx_dmc_model_v1;

/** @brief 创建控制器句柄的配置。 */
typedef struct szx_dmc_config_v1
{
    uint32_t struct_size;                 /**< 结构体大小。 */
    uint32_t abi_version;                 /**< ABI 版本。 */
    uint16_t model;                       /**< 驱动系列。 */
    uint16_t card_index;                  /**< 板卡编号。 */
    uint8_t simulation;                   /**< 是否启用仿真。 */
    uint8_t allow_simulation_fallback;    /**< 是否允许回退仿真。 */
    uint32_t compare_queue_capacity;      /**< 位置比较队列容量。 */
    uint32_t input_count;                 /**< 数字输入数量。 */
    uint32_t output_count;                /**< 数字输出数量。 */
    char profile_id[64];                  /**< 型号配置标识，例如 E3032。 */
    szx_dmc_log_callback_v1 log_callback; /**< 日志回调；为空时由库输出到控制台。 */
    void *log_user_data;                  /**< 传递给日志回调的用户数据指针。 */
} szx_dmc_config_v1;

/** @brief 采集请求中的位置比较点。 */
typedef struct szx_dmc_compare_point_v1
{
    double position;  /**< 控制器单位的位置。 */
    uint32_t ordinal; /**< 帧序号。 */
    uint16_t row;     /**< 扫描行号。 */
    uint16_t column;  /**< 扫描列号。 */
} szx_dmc_compare_point_v1;

/** @brief 版本化的控制器和采集状态。 */
typedef struct szx_dmc_state_v1
{
    uint32_t struct_size;                            /**< 结构体大小。 */
    uint32_t abi_version;                            /**< ABI 版本。 */
    uint16_t state;                                  /**< 控制器状态。 */
    uint16_t last_error;                             /**< 最近错误。 */
    uint64_t operation_id;                           /**< 采集任务编号。 */
    uint32_t current_row;                            /**< 当前扫描行。 */
    uint32_t total_rows;                             /**< 扫描总行数。 */
    uint32_t compared_triggers;                      /**< 已确认触发数。 */
    uint32_t received_frames;                        /**< 已保存帧数。 */
    uint32_t expected_frames;                        /**< 计划帧数。 */
    uint64_t start_edge_sequence;                    /**< 启动边沿序号。 */
    int64_t last_start_edge_timestamp_nanoseconds;   /**< 启动边沿时间戳。 */
    uint32_t compare_queue_remaining;                /**< 剩余比较点数。 */
    uint8_t safety_latched;                          /**< 是否锁存安全停止。 */
    uint8_t compare_enabled;                         /**< 是否启用位置比较。 */
    uint8_t input_valid;                             /**< 输入采样是否有效。 */
    uint8_t hardware;                                /**< 是否使用实体板卡。 */
    uint16_t model;                                  /**< 驱动系列。 */
    char diagnostic[SZX_DMC_DIAGNOSTIC_CAPACITY_V1]; /**< 最近一次操作的诊断文本，以零结尾。 */
} szx_dmc_state_v1;

/** @brief 基本轴或 IO 命令。 */
typedef struct szx_dmc_command_v1
{
    uint32_t struct_size; /**< 结构体大小。 */
    uint32_t abi_version; /**< ABI 版本。 */
    uint16_t action;      /**< 命令动作：0 使能，1 禁用，2 回零，3 绝对，4 相对，5 减速停止，6 急停。 */
    uint16_t axis;        /**< 目标轴。 */
    double value;         /**< 命令数值。 */
    double min_velocity;  /**< 最小速度。 */
    double max_velocity;  /**< 最大速度。 */
    double acceleration;  /**< 加速度。 */
    double deceleration;  /**< 减速度。 */
} szx_dmc_command_v1;

/** @brief 包含位置比较点和扫描位置的采集请求。 */
typedef struct szx_dmc_capture_v1
{
    uint32_t struct_size;                           /**< 结构体大小。 */
    uint32_t abi_version;                           /**< ABI 版本。 */
    uint64_t operation_id;                          /**< 采集任务编号。 */
    uint16_t start_source;                          /**< 启动来源位标志：1 软件命令，2 外部硬件触发。 */
    uint16_t start_input;                           /**< 启动输入。 */
    uint8_t start_input_active_level;               /**< 启动输入有效电平。 */
    uint16_t trigger_source;                        /**< 帧触发来源。 */
    uint16_t scan_axis;                             /**< 扫描轴。 */
    uint16_t row_axis;                              /**< 换行轴。 */
    uint16_t compare_output;                        /**< 比较输出。 */
    uint8_t compare_output_active_level;            /**< 比较输出有效电平。 */
    uint32_t expected_frames;                       /**< 计划帧数。 */
    uint32_t compare_pulse_cycles;                  /**< 比较脉冲宽度。 */
    uint32_t compare_queue_capacity;                /**< 位置比较队列容量。 */
    const szx_dmc_compare_point_v1 *compare_points; /**< 比较点数组。 */
    uint32_t compare_point_count;                   /**< 比较点数量。 */
    const double *scan_positions;                   /**< 列位置数组。 */
    uint32_t scan_position_count;                   /**< 列位置数量。 */
    const double *row_positions;                    /**< 行位置数组。 */
    uint32_t row_position_count;                    /**< 行位置数量。 */
    double scan_start_position;                     /**< 扫描起点。 */
    double scan_end_position;                       /**< 扫描终点。 */
    double scan_speed;                              /**< 扫描速度。 */
    double row_speed;                               /**< 换行速度。 */
} szx_dmc_capture_v1;

/**
 * @brief 返回动态库支持的 ABI 版本。
 * @return 当前支持的 ABI 主版本号。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_abi_version_v1(void) SZX_DMC_NOEXCEPT;
/**
 * @brief 根据版本化配置创建控制器句柄。
 * @param[in] config 创建配置；必须填写 struct_size 和 abi_version。
 * @param[out] out_handle 返回新建句柄，失败时写入 NULL。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_create_v1(const szx_dmc_config_v1 *config, szx_dmc_handle **out_handle) SZX_DMC_NOEXCEPT;
/**
 * @brief 销毁控制器句柄并释放资源。
 * @param[in] handle 要销毁的有效句柄。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_destroy_v1(szx_dmc_handle *handle) SZX_DMC_NOEXCEPT;
/**
 * @brief 连接所选板卡或仿真适配器。
 * @param[in] handle 已创建的控制器句柄。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_connect_v1(szx_dmc_handle *handle) SZX_DMC_NOEXCEPT;
/**
 * @brief 断开板卡并释放资源。
 * @param[in] handle 已创建的控制器句柄。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_disconnect_v1(szx_dmc_handle *handle) SZX_DMC_NOEXCEPT;
/**
 * @brief 轮询控制器状态并写入调用方结构体。
 * @param[in] handle 已创建的控制器句柄。
 * @param[in,out] state 输出状态结构体；调用前必须填写兼容的大小和 ABI 版本。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_poll_state_v1(szx_dmc_handle *handle, szx_dmc_state_v1 *state) SZX_DMC_NOEXCEPT;
/**
 * @brief 执行基本轴或 IO 命令。
 * @param[in] handle 已创建的控制器句柄。
 * @param[in] command 要执行的命令及运动参数。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_execute_v1(szx_dmc_handle *handle, const szx_dmc_command_v1 *command) SZX_DMC_NOEXCEPT;
/**
 * @brief 装载采集请求，并在返回前复制指针数组。
 * @param[in] handle 已创建的控制器句柄。
 * @param[in] capture 采集请求及其比较点、扫描位置数组。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 * @note 调用返回后，capture 中的数组可以由调用方释放或复用。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_arm_capture_v1(szx_dmc_handle *handle, const szx_dmc_capture_v1 *capture) SZX_DMC_NOEXCEPT;
/**
 * @brief 启动软件触发的采集任务。
 * @param[in] handle 已创建的控制器句柄。
 * @param[in] operation_id 已通过 arm_capture 装载的任务编号。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_start_capture_v1(szx_dmc_handle *handle, uint64_t operation_id) SZX_DMC_NOEXCEPT;
/**
 * @brief 按指定来源启动采集任务。
 * @param[in] handle 已创建的控制器句柄。
 * @param[in] operation_id 已通过 arm_capture 装载的任务编号。
 * @param[in] source 启动来源位标志：1 软件命令，2 外部硬件触发。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_start_capture_source_v1(szx_dmc_handle *handle, uint64_t operation_id,
                                                     uint16_t source) SZX_DMC_NOEXCEPT;
/**
 * @brief 取消当前采集任务，可选执行急停。
 * @param[in] handle 已创建的控制器句柄。
 * @param[in] emergency 非零时执行急停并锁存安全状态。
 * @return SZX_DMC_SUCCESS 或具体错误码。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_cancel_capture_v1(szx_dmc_handle *handle, uint8_t emergency) SZX_DMC_NOEXCEPT;
/**
 * @brief 将最近诊断信息复制到调用方缓冲区。
 * @param[in] handle 已创建的控制器句柄。
 * @param[out] buffer 接收以零结尾的诊断文本的缓冲区，可传 NULL 查询所需容量。
 * @param[in] capacity buffer 的字节容量。
 * @param[out] required_size 返回包含结尾零字符所需的字节数，可传 NULL。
 * @return SZX_DMC_SUCCESS；空间不足时返回 SZX_DMC_BUFFER_TOO_SMALL。
 */
SZX_DMC_C_API
int32_t SZX_DMC_CALL szx_dmc_last_error_v1(szx_dmc_handle *handle, char *buffer, size_t capacity,
                                           size_t *required_size) SZX_DMC_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef SZX_DMC_NOEXCEPT
