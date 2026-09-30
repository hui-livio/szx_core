#pragma once

/**
 * @file DmcTypes.hpp
 * @brief C++ 接口和各控制器适配器共用的数据类型。
 *
 * 这些类型与硬件无关，profileId 用于选择具体型号配置。
 * 具体型号的行为由 DmcController 的实现负责。
 */

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * @brief SZX DMC 公共 C++ 类型命名空间。
 *
 * 该命名空间中的类型可由控制器、采集会话和上层语言绑定共同使用。
 */
namespace SZX::DMC
{
inline namespace v1
{

/** @brief 稳定的 DMC 操作错误码。 */
enum class DmcError : std::uint16_t
{
    Success              = 0,  ///< 操作成功。
    InvalidArgument      = 1,  ///< 参数或配置无效。
    DriverUnavailable    = 2,  ///< 驱动不可用。
    ArchitectureMismatch = 3,  ///< 驱动与当前进程架构不匹配。
    SymbolUnavailable    = 4,  ///< 驱动缺少所需的函数符号。
    BoardNotFound        = 5,  ///< 找不到指定的控制器板卡。
    NotConnected         = 6,  ///< 控制器未连接。
    Busy                 = 7,  ///< 控制器正忙。
    SafetyLatched        = 8,  ///< 安全停止已锁存。
    AxisNotReady         = 9,  ///< 轴未就绪。
    NotHomed             = 10, ///< 轴尚未回零。
    LimitReached         = 11, ///< 到达运动限位。
    MotionTimeout        = 12, ///< 运动超时。
    IoUnavailable        = 13, ///< 输入或输出不可用。
    CompareUnavailable   = 14, ///< 位置比较不可用。
    CompareQueueFull     = 15, ///< 位置比较队列容量不足。
    TriggerTimeout       = 16, ///< 等待触发超时。
    FrameProgressInvalid = 17, ///< 相机帧进度无效。
    Cancelled            = 18, ///< 任务已取消。
    ResourceExhausted    = 19, ///< 资源不足。
    UnexpectedFailure    = 20, ///< 未预期的实现错误。
    BufferTooSmall       = 21, ///< 输出缓冲区太小。
};

/** @brief 控制器和采集任务的生命周期状态。 */
enum class DmcState : std::uint8_t
{
    Disconnected = 0, ///< 未连接。
    Ready,            ///< 已连接且空闲。
    Armed,            ///< 采集已装载。
    WaitingForStart,  ///< 等待启动输入。
    Preparing,        ///< 准备扫描。
    Scanning,         ///< 正在扫描。
    WaitingForFrames, ///< 等待相机帧。
    Completed,        ///< 采集完成。
    Cancelled,        ///< 任务已取消。
    Failed,           ///< 操作失败。
    SafetyLatched,    ///< 安全停止已锁存。
};

/**
 * @brief 采集任务允许的启动来源。
 *
 * 该类型是位标志，可同时允许软件命令和外部硬件触发；DMC 不解析硬件
 * 触发来自哪个按钮，调用方负责把业务事件转换为启动调用。
 */
enum class StartSource : std::uint8_t
{
    None            = 0,                                 ///< 不允许启动。
    SoftwareCommand = 1u << 0,                           ///< 软件命令启动。
    HardwareTrigger = 1u << 1,                           ///< 外部硬件触发启动。
    DigitalInput    = HardwareTrigger,                   ///< 兼容旧名称；不表示具体输入接线。
    Any             = SoftwareCommand | HardwareTrigger, ///< 允许两种启动来源。
};

constexpr auto operator|(StartSource left, StartSource right) noexcept -> StartSource
{
    return static_cast<StartSource>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

constexpr auto hasStartSource(StartSource value, StartSource source) noexcept -> bool
{
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(source)) == static_cast<std::uint8_t>(source);
}

/** @brief 相机帧触发来源。 */
enum class FrameTriggerSource : std::uint8_t
{
    Software = 0,          ///< 软件触发。
    PositionCompareOutput, ///< 位置比较输出触发。
};

/** @brief 轴停止方式。 */
enum class StopMode : std::uint8_t
{
    Decelerated = 0, ///< 减速停止。
    Emergency,       ///< 急停并锁存安全状态。
};

/** @brief DMC 驱动系列，具体雷赛型号通过 profileId 区分。 */
enum class DmcModel : std::uint8_t
{
    LeiSaiLTDMC  = 0,           ///< 雷赛 LTDMC 系列。
    Custom       = 255,         ///< 自定义控制器系列。
    GenericLTDMC = LeiSaiLTDMC, ///< LTDMC 兼容别名。
};

/** @brief 日志回调的严重程度。 */
enum class DmcLogLevel : std::uint8_t
{
    Debug = 0, ///< 调试信息。
    Trace,     ///< 运行轨迹。
    Info,      ///< 运行信息。
    Warning,   ///< 警告信息。
    Error,     ///< 错误信息。
};

/**
 * @brief 日志回调；未设置时输出到控制台。
 * @param[in] level 日志级别。
 * @param[in] message 日志文本。
 */
using LogSink = std::function<void(DmcLogLevel level, const std::string &message)>;

using AxisId       = std::uint16_t; ///< 轴编号，从零开始计数。
using InputId      = std::uint16_t; ///< 数字输入编号，从零开始计数。
using OutputId     = std::uint16_t; ///< 数字输出编号，从零开始计数。
using OperationId  = std::uint64_t; ///< 采集任务的唯一编号。
using FrameOrdinal = std::uint32_t; ///< 采集帧序号，从零开始计数。

struct InputSnapshot;

/** @brief 原始数字输入快照回调；DMC 不解析输入的业务含义。 */
using InputEventSink = std::function<void(const InputSnapshot &snapshot)>;

/** @brief 控制器创建配置。 */
struct DmcConfig
{
    DmcModel model                     = DmcModel::LeiSaiLTDMC; ///< 驱动系列。
    std::string profileId              = "E3032";               ///< 型号配置标识。
    std::uint16_t cardIndex            = 0;                     ///< 板卡编号。
    bool simulation                    = true;                  ///< 是否仿真。
    bool allowSimulationFallback       = false;                 ///< 是否允许回退仿真。
    std::uint32_t compareQueueCapacity = 256;                   ///< 位置比较队列容量。
    std::uint32_t inputCount           = 32;                    ///< 数字输入数量。
    std::uint32_t outputCount          = 32;                    ///< 数字输出数量。
    LogSink logSink{};                                          ///< 日志回调；为空时输出到控制台。
    InputEventSink inputEventSink{};                            ///< 原始输入电平变化回调。
};

/** @brief 控制器解析后的型号配置。 */
struct DmcProfile
{
    DmcModel model                = DmcModel::LeiSaiLTDMC; ///< 驱动系列。
    std::string id                = "E3032";               ///< 配置标识。
    AxisId defaultScanAxis        = 0;                     ///< 默认扫描轴。
    AxisId defaultRowAxis         = 1;                     ///< 默认换行轴。
    OutputId defaultCompareOutput = 0;                     ///< 默认比较输出。
    std::uint16_t axisCount       = 2;                     ///< 轴数量。
    bool supportsPositionCompare  = true;                  ///< 是否支持位置比较。
};

/** @brief 点位运动的速度和加减速参数。 */
struct MotionProfile
{
    double minVelocity  = 0.0; ///< 最小速度。
    double maxVelocity  = 0.0; ///< 最大速度。
    double acceleration = 0.0; ///< 加速度。
    double deceleration = 0.0; ///< 减速度。
    double stopVelocity = 0.0; ///< 停止速度。
};

/** @brief 轴回零参数。 */
struct HomeProfile
{
    std::uint16_t mode  = 0;   ///< 回零模式。
    double lowVelocity  = 0.0; ///< 回零低速。
    double highVelocity = 0.0; ///< 回零高速。
    double acceleration = 0.0; ///< 加速度。
    double deceleration = 0.0; ///< 减速度。
    double offset       = 0.0; ///< 回零偏移。
};

/** @brief 位置比较触发点。 */
struct ComparePoint
{
    double position       = 0.0; ///< 位置。
    std::uint32_t ordinal = 0;   ///< 帧序号。
    std::uint16_t row     = 0;   ///< 行号。
    std::uint16_t column  = 0;   ///< 列号。
};

/** @brief 位置比较队列配置。 */
struct CompareConfig
{
    AxisId axis                 = 0;    ///< 轴编号。
    OutputId output             = 0;    ///< 输出编号。
    bool activeLevel            = true; ///< 有效电平。
    std::uint16_t source        = 0;    ///< 比较源。
    std::uint32_t pulseCycles   = 0;    ///< 脉冲周期数。
    std::uint32_t queueCapacity = 0;    ///< 队列容量。
    std::vector<ComparePoint> points;   ///< 比较点列表。
};

/** @brief 行列扫描运动计划。 */
struct ScanPlan
{
    AxisId scanAxis = 0;               ///< 扫描轴。
    AxisId rowAxis  = 1;               ///< 换行轴。
    std::vector<double> scanPositions; ///< 列位置列表。
    std::vector<double> rowPositions;  ///< 行位置列表。
    double scanStartPosition = 0.0;    ///< 扫描起点。
    double scanEndPosition   = 0.0;    ///< 扫描终点。
    double scanSpeed         = 0.0;    ///< 扫描速度。
    double rowSpeed          = 0.0;    ///< 换行速度。
    MotionProfile scanProfile{};       ///< 扫描轴的速度和加减速参数。
    MotionProfile rowProfile{};        ///< 换行轴的速度和加减速参数。
};

/** @brief 采集任务请求。 */
struct CaptureRequest
{
    OperationId operationId            = 0;                                         ///< 采集任务编号。
    StartSource startSource            = StartSource::Any;                          ///< 允许的启动来源位标志。
    InputId startInput                 = 0;                                         ///< 启动输入。
    bool startInputActiveLevel         = true;                                      ///< 启动有效电平。
    FrameTriggerSource triggerSource   = FrameTriggerSource::PositionCompareOutput; ///< 帧触发来源。
    std::uint32_t expectedFrames       = 0;                                         ///< 计划帧数。
    std::uint16_t compareOutput        = 0;                                         ///< 比较输出。
    bool compareOutputActiveLevel      = true;                                      ///< 比较输出有效电平。
    std::uint16_t compareSource        = 0;                                         ///< 比较源。
    std::uint32_t comparePulseCycles   = 0;                                         ///< 比较脉冲宽度。
    std::uint32_t compareQueueCapacity = 0;                                         ///< 位置比较队列容量。
    std::vector<ComparePoint> comparePoints;                                        ///< 比较点列表。
    ScanPlan scanPlan{};                                                            ///< 扫描轴和换行轴的运动计划。
};

/** @brief 单轴状态快照。 */
struct AxisStatus
{
    AxisId axis             = 0;     ///< 轴编号。
    double position         = 0.0;   ///< 位置。
    double targetPosition   = 0.0;   ///< 目标位置。
    double velocity         = 0.0;   ///< 速度。
    bool enabled            = false; ///< 是否使能。
    bool homed              = false; ///< 是否回零。
    bool moving             = false; ///< 是否运动。
    bool homing             = false; ///< 是否回零中。
    std::uint16_t errorCode = 0;     ///< 轴错误码。
};

/** @brief 数字输入采样快照。 */
struct InputSnapshot
{
    std::uint64_t activeBits          = 0;     ///< 有效输入位。
    std::uint64_t sampledBits         = 0;     ///< 采样输入位。
    std::int64_t timestampNanoseconds = 0;     ///< 采样时间戳。
    bool valid                        = false; ///< 采样是否有效。
};

/** @brief 控制器和采集任务状态快照。 */
struct DmcStatus
{
    DmcState state                                 = DmcState::Disconnected; ///< 生命周期状态。
    DmcError lastError                             = DmcError::Success;      ///< 最近错误。
    OperationId operationId                        = 0;                      ///< 采集任务编号。
    std::uint32_t currentRow                       = 0;                      ///< 当前行。
    std::uint32_t totalRows                        = 0;                      ///< 总行数。
    std::uint32_t comparedTriggers                 = 0;                      ///< 已确认触发数。
    std::uint32_t receivedFrames                   = 0;                      ///< 已保存帧数。
    std::uint32_t expectedFrames                   = 0;                      ///< 计划帧数。
    std::uint64_t startEdgeSequence                = 0;                      ///< 启动边沿序号。
    std::int64_t lastStartEdgeTimestampNanoseconds = 0;                      ///< 启动边沿时间戳。
    std::uint32_t compareQueueRemaining            = 0;                      ///< 剩余比较点数。
    bool safetyLatched                             = false;                  ///< 安全停止是否锁存。
    bool compareEnabled                            = false;                  ///< 比较输出是否启用。
    bool inputValid                                = false;                  ///< 输入采样是否有效。
    bool hardware                                  = false;                  ///< 是否使用硬件。
    DmcModel model                                 = DmcModel::GenericLTDMC; ///< 驱动系列。
    std::string profileId;                                                   ///< 型号配置标识。
    std::string diagnostic;                                                  ///< 诊断信息。
    std::vector<AxisStatus> axes;                                            ///< 轴状态列表。
};

} // namespace v1
} // namespace SZX::DMC
