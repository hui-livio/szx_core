/**
 * @file CameraTypes.h
 * @brief 相机基础类型 — 定义跨 DLL 安全的枚举和 POD 数据结构。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraExport.h"

#include <cstddef>
#include <cstdint>
#include <string>


namespace SZXCamera
{

/// @brief 相机后端类型
enum class CameraBackend
{
    CV,   ///< OpenCV 模拟相机
    IKAP, ///< IKAP 相机
    HUARAY,     ///< Huaray IMVFG frame-grabber cameras
    HUARAY_USB, ///< Huaray IMV USB/GigE cameras
};

struct CameraServiceConfig
{
    CameraBackend backend{CameraBackend::IKAP}; ///< 相机后端类型
    std::string cvImageDirectory;               ///< CV 后端读取的本地图片目录
};

// ========================================================================
// 跨 DLL 安全的基础枚举与 POD 结构体
// ========================================================================

enum class CameraRole : uint8_t
{
    Unknown = 0, ///< 未知角色
    Top,         ///< 顶视相机
    Side,        ///< 侧视相机
    External,    ///< 外部相机
    Other        ///< 其他角色
};

enum class CameraState : uint8_t
{
    Created = 0, ///< 对象已创建，尚未发现设备
    Discovered,  ///< 已枚举到设备
    Opened,      ///< 已打开
    Configured,  ///< 参数已下发
    Streaming,   ///< 采集中
    Stopped,     ///< 已停止/关闭
    Error        ///< 错误状态
};

enum class TriggerMode : uint8_t
{
    Continuous = 0, ///< 连续采集
    Software,       ///< 软件触发
    Hardware        ///< 硬件触发
};

enum class PixelFormat : uint8_t
{
    Unknown = 0,   ///< 未知格式
    Mono8,         ///< 单通道 8 位灰度
    Gray8 = Mono8, ///< 单通道 8 位灰度
    RGB8,          ///< 3 通道 RGB 8 位
    BGR8,          ///< 3 通道 BGR 8 位
    YUYV,          ///< YUYV 格式
    UYVY,          ///< UYVY 格式
    YUV420P,       ///< 420P 格式
};

struct CameraInfo
{
    const char *deviceId;                 ///< 设备 ID（指针，生命周期由持有者保证）
    const char *serialNumber;             ///< 序列号（指针，生命周期由持有者保证）
    const char *logicalName;              ///< 逻辑名称（指针，生命周期由持有者保证）
    const char *vendor;                   ///< 厂商（指针，生命周期由持有者保证）
    const char *model;                    ///< 模型（指针，生命周期由持有者保证）
    CameraRole role{CameraRole::Unknown}; ///< 相机角色
};

struct CameraConfig
{
    double exposureTime{0.0};                         ///< 曝光时间（秒）
    double gain{0.0};                                 ///< 增益（dB）
    TriggerMode triggerMode{TriggerMode::Continuous}; ///< 触发模式
    PixelFormat pixelFormat{PixelFormat::Unknown};    ///< 图像格式
    int bufferCount{8};                               ///< 缓冲区数量
    bool reverseX{false};                             ///< 是否反转 X 轴
    bool reverseY{false};                             ///< 是否反转 Y 轴
};

struct CameraRuntimeState
{
    CameraState state{CameraState::Created}; ///< 相机运行状态
    const char *lastError{nullptr};          ///< 最近错误信息（指针，生命周期由持有者保证）
    std::uint64_t totalFrames{0};            ///< 总帧数
    std::int64_t lastFrameTimeMs{0};         ///< 最近收帧时间戳 (ms since epoch)
    std::uint64_t lastFrameLatencyUs{0};     ///< 最近帧延迟（微秒）
    std::uint64_t maxFrameLatencyUs{0};      ///< 最大帧延迟（微秒）
};

struct Meta
{
    const char *cameraName{nullptr};         ///< 相机名称（指针，生命周期由持有者保证）
    const char *cameraSerialNumber{nullptr}; ///< 相机序列号（指针，生命周期由持有者保证）
    // Producer-defined monotonic source sequence.  IKAP uses the SDK block
    // ID when available and falls back to a local sequence otherwise.
    // Keep this as the sole frame-order field: Meta crosses the camera DLL ABI.
    std::uint64_t frameId{0};     ///< 帧源序号
    std::uint64_t timestampMs{0}; ///< 时间戳 (ms)
    double height{0.0};           ///< 图像高度（米）
    bool highExposure{false};     ///< 是否为高曝光模式
    const char *taskId{nullptr};  ///< 任务 ID（指针，生命周期由持有者保证）
};

struct FramePacket
{
    Meta meta;                                     ///< 帧元数据
    int width{0};                                  ///< 图像宽度
    int height{0};                                 ///< 图像高度
    int channels{0};                               ///< 图像通道数
    PixelFormat pixelFormat{PixelFormat::Unknown}; ///< 图像格式
    const std::uint8_t *data{nullptr};             ///< 图像数据（生命周期由内部管理）
    std::size_t dataSize{0};                       ///< 图像数据大小（字节）
};

/// @brief 采集管道统计信息
struct CapturePipelineStats
{
    std::size_t sinkCount{0};           ///< 接收者数量
    std::uint64_t publishedFrames{0};   ///< 已发布帧数
    std::uint64_t deliveredFrames{0};   ///< 已交付帧数
    std::uint64_t emptyPublishCount{0}; ///< 空 publish 数量
    std::uint64_t totalPublishNanos{0}; ///< 总 publish 时间 (ns)
    std::uint64_t maxPublishNanos{0};   ///< 最大 publish 时间 (ns)
    std::uint64_t slowPublishCount{0};  ///< 慢速 publish 数量
    std::uint64_t lastPublishTimeMs{0}; ///< 最近 publish 时间戳 (ms since epoch)
};

} // namespace SZXCamera
