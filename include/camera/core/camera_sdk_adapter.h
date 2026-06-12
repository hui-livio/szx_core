#pragma once

#include "camera_types.h"
#include "camera/camera_core_api.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#define INFINITE            0xFFFFFFFF  // Infinite timeout
namespace SZXCamera{
    /**
     * ICameraSdkAdapter 表示“一台具体相机”和“某个厂商 SDK”之间的最小适配接口。
     *
     * 设计原则：
     * 1. 上层不要直接依赖厂商 SDK 的句柄、回调签名、错误码。
     * 2. 适配器只解决设备接入问题，不承担业务流程、PLC 联动、文件命名等职责。
     * 3. 每个适配器实例只负责一台相机，便于管理生命周期和错误恢复。
     */
    class CAMERA_CORE_API ICameraSdkAdapter
    {
    public:
        /**
         * FrameCallback 是统一的帧回调类型。
         *
         * 含义：
         * - 当底层 SDK 采到一帧后，适配器通过该回调把统一格式的 FramePacket 交给上层。
         * - 这里按值传递 FramePacket，是为了便于后续做 move 转移和异步处理。
         */
        using FrameCallback = std::function<void(FramePtr)>;

        virtual ~ICameraSdkAdapter() = default;

        /**
         * 枚举相机设备。
         *
         * 功能：
         * - 调用厂商 SDK 的全局枚举能力。
         * - 将枚举结果转换成统一的 CameraInfo 列表。
         *
         * @param errorMessage 输出参数。
         *        当枚举失败时，写入可读错误信息；成功时通常为空。
         *
         * @return std::vector<CameraInfo> 枚举到的相机设备列表。
            *         返回空列表不一定代表失败，也可能表示当前确实没有设备在线。
         */
        virtual std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) = 0;

        /**
         * 打开相机设备。
         *
         * 功能：
         * - 完成底层设备连接或句柄初始化。
         * - 打开成功后，后续才能下发参数或启动采集。
         *
         * @param errorMessage 输出参数。
         *        当打开失败时，写入可读错误信息；成功时通常为空。
         *
         * @return true 表示打开成功。
         * @return false 表示打开失败。
         */
        virtual bool open(std::string *errorMessage = nullptr) = 0;

        /**
         * 关闭相机设备。
         *
         * 功能：
         * - 释放底层句柄、流对象或相关资源。
         * - 如果设备正在采集，通常应先停止采集再关闭。
         *
         * @param errorMessage 输出参数。
         *        当关闭失败时，写入可读错误信息；成功时通常为空。
         *
         * @return true 表示关闭成功。
         * @return false 表示关闭失败。
         */
        virtual bool close(std::string *errorMessage = nullptr) = 0;

        /**
         * 下发相机配置参数。
         *
         * 功能：
         * - 将统一的 CameraConfig 翻译成底层 SDK 的具体参数调用。
         * - 常见内容包括曝光、增益、触发模式、像素格式、缓冲区数量等。
         *
         * @param config 期望应用到该相机的配置对象。
         * @param errorMessage 输出参数。
         *        当配置失败时，写入可读错误信息；成功时通常为空。
         *
         * @return true 表示配置下发成功。
         * @return false 表示配置下发失败。
         */
        virtual bool applyConfig(const CameraConfig &config, std::string *errorMessage = nullptr) = 0;

        /**
         * 启动采集。
         *
         * 功能：
         * - 让相机开始出图。
         * - 对连续采集模式，通常意味着打开数据流。
         * - 对触发模式，通常意味着进入等待触发的工作状态。
         *
         * @param errorMessage 输出参数。
         *        当启动失败时，写入可读错误信息；成功时通常为空。
         *
         * @return true 表示开始采集成功。
         * @return false 表示开始采集失败。
         */
        virtual bool startGrabbing(std::string *errorMessage = nullptr) = 0;

        /**
         * 停止采集。
         *
         * 功能：
         * - 停止相机流输出或停止等待触发。
         * - 通常在关闭相机前或任务结束时调用。
         *
         * @param errorMessage 输出参数。
         *        当停止失败时，写入可读错误信息；成功时通常为空。
         *
         * @return true 表示停止采集成功。
         * @return false 表示停止采集失败。
         */
        virtual bool stopGrabbing(std::string *errorMessage = nullptr) = 0;

        /**
         * 执行一次软件触发。
         *
         * 功能：
         * - 当相机处于软件触发模式并已开始采集时，主动发送一次软触发命令。
         * - 对不支持软件触发的适配器，可以返回失败并给出明确错误信息。
         *
         * @param errorMessage 输出参数。
         *        当执行失败时，写入可读错误信息；成功时通常为空。
         *
         * @return true 表示软件触发命令发送成功。
         * @return false 表示发送失败。
         */
        virtual bool executeSoftwareTrigger(std::string *errorMessage = nullptr) = 0;


        /**
         * 设置帧回调函数。
         *
         * 功能：
         * - 注册一个上层回调，当底层 SDK 得到一帧图像时回调该函数。
         * - 一般在设备对象初始化时注册一次。
         *
         * @param callback 上层帧处理回调。
         *        若为空，则表示当前不处理帧输出。
         */
        virtual void setFrameCallback(FrameCallback callback) = 0;

        /**
         * 获取当前适配器所对应的相机基础信息。
         *
         * @return CameraInfo 相机标识、序列号、逻辑名、厂商、型号等信息。
         */
        virtual CameraInfo info() const = 0;
    };

    /**
     * ICameraSdkFactory 负责“枚举设备”和“创建设备适配器”。
     *
     * 为什么单独拆一个工厂接口：
     * - 枚举设备通常不是单台相机行为，而是 SDK 的全局行为。
     * - Manager 只依赖工厂，就能在后续轻松切换成 IKap、海康或 mock 实现。
     */
    class CAMERA_CORE_API ICameraSdkFactory
    {
    public:
        virtual ~ICameraSdkFactory() = default;

        /**
         * 枚举当前系统中可见的相机设备。
         *
         * 功能：
         * - 调用厂商 SDK 的全局枚举能力。
         * - 将枚举结果转换成统一的 CameraInfo 列表。
         *
         * @param errorMessage 输出参数。
         *        当枚举失败时，写入可读错误信息；成功时通常为空。
         *
         * @return std::vector<CameraInfo> 枚举到的设备列表。
         *         返回空列表不一定代表失败，也可能表示当前确实没有设备在线。
         */
        virtual std::vector<CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) = 0;

        /**
         * 为指定设备创建一个适配器实例。
         *
         * 功能：
         * - 根据 CameraInfo 创建对应的一台相机适配器。
         * - 适配器实例后续将交给 CameraDevice 管理。
         *
         * @param info 目标设备的基础信息。
         * @param errorMessage 输出参数。
         *        当创建失败时，写入可读错误信息；成功时通常为空。
         *
         * @return std::unique_ptr<ICameraSdkAdapter> 创建成功时返回适配器对象。
         *         创建失败时返回 nullptr。
         */
        virtual std::unique_ptr<ICameraSdkAdapter> createAdapter(const CameraInfo &info,
                                                                std::string *errorMessage = nullptr) = 0;
    };

    
}
