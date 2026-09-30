#pragma once

/**
 * @file DmcController.hpp
 * @brief C++ DMC 统一接口和雷赛 LTDMC 工厂适配器。
 *
 * DmcController 是采集会话使用的硬件无关控制器契约。
 * 具体实现可以连接实体板卡，也可以运行在仿真模式。
 */

#include "DmcExport.hpp"
#include "DmcTypes.hpp"

#include <memory>

#include <Result.hpp>

/**
 * @brief SZX DMC 公共控制器接口命名空间。
 */
namespace SZX::DMC
{
inline namespace v1
{

/**
 * @brief 统一的运动、IO 和采集控制器接口。
 *
 * 该类是纯虚接口，请使用 DmcControllerFactory::create()
 * 获取由 DmcConfig 选择的具体控制器实现。
 */
class SZX_DMC_API DmcController
{
public:
    /**
     * @brief 通过工厂创建控制器。
     * @param[in] config 控制器和型号配置。
     * @return 返回多态控制器或稳定的 DmcError 错误码。
     */
    static SZX::COMMON::Result<std::unique_ptr<DmcController>> create(DmcConfig config = {});

    /** @brief 销毁控制器并释放资源。 */
    virtual ~DmcController() noexcept = default;

protected:
    /** @brief 供具体控制器实现调用的默认构造函数。 */
    DmcController() noexcept = default;

public:
    /** @brief 控制器不可复制。 */
    DmcController(const DmcController &) = delete;
    /** @brief 控制器不可复制赋值。 */
    DmcController &operator=(const DmcController &) = delete;

    /**
     * @brief 连接配置的板卡或进入仿真模式。
     * @return 连接成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> connect() = 0;
    /**
     * @brief 断开连接并释放资源。
     * @return 断开成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> disconnect() = 0;
    /**
     * @brief 返回控制器状态快照。
     * @return 包含连接、轴、采集和诊断信息的状态快照。
     */
    virtual SZX::COMMON::Result<DmcStatus> status() const = 0;
    /**
     * @brief 返回解析后的型号配置。
     * @return 当前控制器使用的驱动系列和型号参数。
     */
    virtual SZX::COMMON::Result<DmcProfile> profile() const = 0;

    // 日志由上层应用注入，DMC 模块不依赖 szx::logger，便于 Python 等适配器使用。
    /**
     * @brief 设置日志回调；传入空回调时输出到控制台。
     * @param[in] sink 日志接收函数；为空时恢复默认控制台输出。
     */
    virtual void setLogSink(LogSink sink) = 0;

    /**
     * @brief 设置硬件按钮事件回调。
     * @param[in] sink 按钮按下事件接收函数；为空时停止通知。
     * @note 回调可能在输入轮询线程运行；实现会在释放控制器锁后调用。
     */
    virtual void setButtonEventSink(ButtonEventSink sink) = 0;

    /**
     * @brief 使能指定轴。
     * @param[in] axis 要使能的轴编号。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> enableAxis(AxisId axis) = 0;
    /**
     * @brief 禁用指定轴。
     * @param[in] axis 要禁用的轴编号。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> disableAxis(AxisId axis) = 0;
    /**
     * @brief 执行指定轴回零。
     * @param[in] axis 要回零的轴编号。
     * @param[in] profile 回零模式和速度参数。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> homeAxis(AxisId axis, const HomeProfile &profile = {}) = 0;
    /**
     * @brief 执行绝对位置运动。
     * @param[in] axis 要运动的轴编号。
     * @param[in] position 目标绝对位置。
     * @param[in] profile 速度和加减速参数。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> moveAbsolute(AxisId axis, double position, const MotionProfile &profile = {}) = 0;
    /**
     * @brief 执行相对位置运动。
     * @param[in] axis 要运动的轴编号。
     * @param[in] distance 相对于当前位置的位移。
     * @param[in] profile 速度和加减速参数。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> moveRelative(AxisId axis, double distance, const MotionProfile &profile = {}) = 0;
    /**
     * @brief 按指定方式停止轴运动。
     * @param[in] axis 要停止的轴编号。
     * @param[in] mode 停止方式；急停会锁存安全状态。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> stop(AxisId axis, StopMode mode = StopMode::Decelerated) = 0;
    /**
     * @brief 执行急停并锁存安全状态。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> emergencyStop() = 0;

    /**
     * @brief 写入数字输出。
     * @param[in] output 要写入的输出编号。
     * @param[in] active true 表示输出有效，false 表示输出无效。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> writeOutput(OutputId output, bool active) = 0;
    /**
     * @brief 读取数字输入快照。
     * @return 当前输入位图、采样时间和有效性。
     */
    virtual SZX::COMMON::Result<InputSnapshot> readInputs() const = 0;

    /**
     * @brief 配置位置比较队列。
     * @param[in] config 比较轴、输出和触发点配置。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> configureCompare(const CompareConfig &config) = 0;
    /**
     * @brief 启用或禁用位置比较输出。
     * @param[in] enabled true 启用比较输出，false 禁用比较输出。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> enableCompare(bool enabled) = 0;
    /**
     * @brief 清空位置比较队列。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> clearCompare() = 0;
    /**
     * @brief 返回已消耗的位置比较触发数。
     * @return 已被控制器确认的比较触发数量。
     */
    virtual SZX::COMMON::Result<std::uint32_t> compareTriggerCount() const = 0;

    /**
     * @brief 装载采集请求但不启动运动。
     * @param[in] request 采集任务、比较点和扫描计划。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> armCapture(const CaptureRequest &request) = 0;
    /**
     * @brief 启动软件触发采集。
     * @param[in] operationId 已通过 armCapture 装载的任务编号。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> startCapture(OperationId operationId) = 0;
    /**
     * @brief 确认已观察到的位置比较触发数。
     * @param[in] operationId 采集任务编号。
     * @param[in] count 本次确认的累计触发数量。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> acknowledgeCompareTrigger(OperationId operationId, std::uint32_t count) = 0;
    /**
     * @brief 上报相机已保存帧数并推进扫描计划。
     * @param[in] operationId 采集任务编号。
     * @param[in] savedFrames 相机已保存的累计帧数。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> acknowledgeFrameProgress(OperationId operationId, FrameOrdinal savedFrames) = 0;
    /**
     * @brief 确认相机完成采集。
     * @param[in] operationId 采集任务编号。
     * @param[in] savedFrames 相机已保存的累计帧数。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> completeCamera(OperationId operationId, FrameOrdinal savedFrames) = 0;
    /**
     * @brief 将当前采集标记为失败。
     * @param[in] operationId 采集任务编号。
     * @param[in] reason 采集失败原因。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> failCapture(OperationId operationId, DmcError reason) = 0;
    /**
     * @brief 取消采集并停止相关轴。
     * @param[in] operationId 采集任务编号。
     * @param[in] mode 停止方式；急停会锁存安全状态。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     */
    virtual SZX::COMMON::Result<void> cancelCapture(OperationId operationId, StopMode mode = StopMode::Decelerated) = 0;
    /**
     * @brief 返回采集任务状态。
     * @param[in] operationId 采集任务编号。
     * @return 当前任务状态快照或稳定的 DmcError 错误码。
     */
    virtual SZX::COMMON::Result<DmcStatus> pollCapture(OperationId operationId) const = 0;

    // 仿真和测试输入注入接口。硬件输入边沿由
    // SDK 适配器采样后进入同一状态机。
    /**
     * @brief 注入一次数字输入采样。
     * @param[in] input 数字输入编号。
     * @param[in] active true 表示输入有效，false 表示输入无效。
     * @return 操作成功返回空结果；失败时返回 DmcError。
     * @note 该接口主要供仿真和测试使用；硬件输入由适配器采样。
     */
    virtual SZX::COMMON::Result<void> signalDigitalInput(InputId input, bool active) = 0;
};

/**
 * @brief 根据驱动系列和 profileId 创建具体控制器。
 *
 * 工厂是公共创建入口，具体型号控制器类型不在公共头文件中暴露。
 */
class SZX_DMC_API DmcControllerFactory
{
public:
    /**
     * @brief 创建型号系列对应的已注册实现。
     * @param[in] config 控制器和型号配置。
     * @return 返回多态控制器或稳定的 DmcError 错误码。
     */
    static SZX::COMMON::Result<std::unique_ptr<DmcController>> create(DmcConfig config = {});
};

/** @brief DmcController 的兼容别名。 */
using Controller = DmcController;

} // namespace v1
} // namespace SZX::DMC
