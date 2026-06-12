#pragma once
#include "camera/camera_core_api.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace SZXCamera{
    /**
     * 相机子系统的基础数据模型统一放在这个头文件里。
     *
     * 设计目标：
     * 1. 让上层业务尽量只依赖这些“中立结构”，不要直接感知厂商 SDK 的句柄和类型。
     * 2. 后续如果切换 SDK，尽量只改适配层，而不是改业务层和 API 层。
     * 3. 把“设备信息、配置、状态、帧元数据”拆开，避免一个大结构承担过多职责。
     */

    /**
     * 标识相机在整套设备中的业务角色。
     * 这个角色不是厂商概念，而是我们自己在业务层对相机用途的分类。
     */
    enum class CameraRole {
        Unknown,   ///< 未知角色。通常用于初始化阶段，或尚未给相机分配业务身份。
        Top,       ///< 顶视相机。通常用于俯视拍照、拼图、高曝光补拍等流程。
        Side,      ///< 侧视相机。通常用于侧面取图或普通外观拍照。
        External,   ///< 外部相机。可用于外接相机、内窥镜或其他特殊用途相机。
        Other      ///< 其他角色。适用于不符合上述分类的特殊相机，或未来新增的角色。
    };

    /**
     * 标识相机设备当前所处的生命周期状态。
     * 这个状态主要用于管理设备是否已经发现、打开、配置或进入采集。
     */
    enum class CameraState {
        Created,     ///< 对象已创建，但还没有进行设备发现或初始化。
        Discovered,  ///< 已经枚举到设备，知道这台相机存在，但尚未真正打开。
        Opened,      ///< 相机已经成功打开，可以继续下发参数或启动采集。
        Configured,  ///< 相机参数已经下发完成，处于可采集的准备状态。
        Streaming,   ///< 相机正在采集，通常表示回调或取流线程已经运行。
        Stopped,     ///< 相机已停止采集或已经关闭，但对象本身仍然存在。
        Error        ///< 相机进入错误状态，通常需要人工处理或重新初始化。
    };

    /**
     * 标识相机当前使用的触发模式。
     * 不同模式决定图像是连续输出还是由软件/硬件信号触发。
     */
    enum class TriggerMode {
        Continuous,  ///< 连续采集模式。相机持续输出图像，不依赖外部触发。
        Software,    ///< 软件触发模式。由程序主动发命令触发一次拍照。
        Hardware     ///< 硬件触发模式。由外部 IO、PLC 或触发线信号触发拍照。
    };

    /**
     * 标识图像的像素格式。
     * 这是对底层 SDK 像素格式的一层统一抽象，便于上层逻辑做兼容。
     */
    enum class PixelFormat {
        Unknown,  ///< 未知像素格式。通常表示尚未配置，或底层格式未映射到统一定义。
        Mono8,    ///< 单通道 8 位灰度图。
        RGB8,     ///< 3 通道 RGB 8 位图像。
        BGR8      ///< 3 通道 BGR 8 位图像。很多 OpenCV 流程会优先使用这种格式。
    };

    /**
     * 描述“这台相机是谁”。
     * 这类信息通常在枚举设备后就可以拿到，生命周期相对稳定。
     */
    struct CAMERA_CORE_API CameraInfo {
        std::string deviceId;      ///< 设备唯一标识。通常来自 SDK 的设备 ID、GUID 或内部索引映射。
        std::string serialNumber;  ///< 相机序列号。适合做设备绑定、持久化标识和排障定位。
        std::string logicalName;   ///< 业务逻辑名称，例如 camera_0、camera_5，供上层统一引用。
        std::string vendor;        ///< 相机厂商名称，例如 Hikrobot、IKap 对应的设备供应商信息。
        std::string model;         ///< 相机型号名称，便于区分不同设备能力和参数范围。
        CameraRole role{CameraRole::Unknown};  ///< 该相机在业务中的角色，例如顶视、侧视、外部相机。
    };

    /**
     * 描述“这台相机应该如何工作”。
     * 这类参数通常来自配置文件或前端 API，下发给具体的 SDK 适配器。
     */
    struct CAMERA_CORE_API CameraConfig {
        double exposureTime{0.0};                     ///< 曝光时间。单位通常由具体 SDK 决定，常见为微秒 us。
        double gain{0.0};                             ///< 增益值。具体范围取决于相机型号和 SDK 能力。
        TriggerMode triggerMode{TriggerMode::Continuous};  ///< 触发模式，决定相机如何开始一次采集。
        PixelFormat pixelFormat{PixelFormat::Unknown};     ///< 期望输出的像素格式，用于统一后续图像处理。
        int bufferCount{8};                           ///< 采集缓冲区数量。缓冲越多越不容易丢帧，但会占用更多内存。
        bool reverseX{false};                         ///< 是否沿 X 方向翻转图像，常用于安装方向修正。
        bool reverseY{false};                         ///< 是否沿 Y 方向翻转图像，常用于安装方向修正。
    };

    /**
     * 描述“这台相机当前运行成什么样了”。
     * 运行态信息会频繁变化，例如状态、错误、最后一帧时间、累计帧数等。
     */
    struct CAMERA_CORE_API CameraRuntimeState {
        CameraState state{CameraState::Created};  ///< 当前生命周期状态，例如已打开、采集中、错误等。
        std::string lastError;                    ///< 最近一次错误信息，便于 API 返回和日志排查。
        std::uint64_t totalFrames{0};             ///< 从本次运行开始累计接收到的总帧数，可用于统计吞吐。
        std::chrono::system_clock::time_point lastFrameTime{};  ///< 最近一次收到帧的时间点，用于判断是否超时或掉线。
         // 最近一次收到帧时，从 frame.timestamp 到 onFrame() 的延迟
        std::uint64_t lastFrameLatencyUs{0};

        // 历史最大帧处理延迟
        std::uint64_t maxFrameLatencyUs{0};
    };

    /**
     * 帧元数据只描述帧的业务上下文，不直接承载像素内容。
     * 这样做的好处是后续无论存图、拼图还是做算法，都可以先看元数据再决定处理策略。
     */
    struct CAMERA_CORE_API FrameMeta {
        std::string cameraName;                           ///< 产生该帧的相机逻辑名称，例如 camera_5。
        std::string cameraSerialNumber;                   ///< 产生该帧的相机序列号，便于追踪具体设备。
        std::uint64_t frameId{0};                         ///< 帧序号。通常用于追踪帧顺序、日志和问题定位。
        std::chrono::system_clock::time_point timestamp{};  ///< 该帧生成或接收的时间戳。
        double height{0.0};                               ///< 采图时关联的高度值，通常来自 PLC 或运动控制系统。
        bool highExposure{false};                         ///< 该帧是否属于高曝光流程，便于后续命名或分流处理。
        std::string taskId;                               ///< 该帧所属任务 ID，用于把多轮任务隔离管理。
    };

    /**
     * 一帧图像的统一承载结构。
     * 当前这里使用 std::vector<uint8_t> 做通用字节缓冲，后续可替换为更高效的自定义缓冲池。
     */
    struct CAMERA_CORE_API FramePacket {
        FrameMeta meta;                               ///< 该帧对应的业务元数据。
        int width{0};                                 ///< 图像宽度，单位为像素。
        int height{0};                                ///< 图像高度，单位为像素。
        int channels{0};                              ///< 图像通道数，例如灰度图通常是 1，彩色图通常是 3。
        PixelFormat pixelFormat{PixelFormat::Unknown};///< 当前帧的实际像素格式。
        std::vector<std::uint8_t> data;               ///< 图像原始字节数据，布局取决于 pixelFormat 和具体约定。
    };
    using FramePtr = std::shared_ptr<const FramePacket>;
}
