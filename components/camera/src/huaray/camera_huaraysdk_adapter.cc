
#include "huaray/camera_huaraysdk_adapter.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Logger.h"

    namespace SZXCamera
{
    namespace
    {

    const char *persistHuarayString(const std::string &value)
    {
        static std::deque<std::string> values;
        values.push_back(value);
        return values.back().c_str();
    }

    void clearError(std::string *errorMessage)
    {
        if (errorMessage)
        {
            errorMessage->clear();
        }
    }

    std::string makeErrorMessage(const std::string &action, long long status)
    {
        std::ostringstream oss;
        oss << action << " failed, status=" << status;
        return oss.str();
    }

    bool startsWithCameraPrefix(const std::string &logicalName)
    {
        return logicalName.rfind("camera_", 0) == 0;
    }

    int tryParseLogicalCameraIndex(const std::string &logicalName)
    {
        if (!startsWithCameraPrefix(logicalName))
        {
            return -1;
        }
        try
        {
            return std::stoi(logicalName.substr(std::string("camera_").size()));
        } catch (...)
        {
            return -1;
        }
    }

    std::string safeStr(const char *s)
    {
        return s ? std::string(s) : std::string();
    }

    /**
         * 在 IMVFG SDK 当前枚举结果里按 CameraInfo 查找一台相机。
         *
         * 匹配优先级：
         *   1. serialNumber
         *   2. deviceId（用 cameraKey 比对）
         *   3. logicalName "camera_N" 退化为 index
         */
    bool find_device(const CameraInfo &info, IMV_FG_DEVICE_INFO &out)
    {
        IMV_FG_DEVICE_INFO_LIST list{};
        if (IMV_FG_EnumDevices(typeInterfaceAll, &list) != IMV_FG_OK)
        {
            return false;
        }
        if (list.nDevNum == 0 || list.pDeviceInfoList == nullptr)
        {
            return false;
        }

        if ((info.serialNumber && info.serialNumber[0]))
        {
            for (unsigned int i = 0; i < list.nDevNum; ++i)
            {
                if (safeStr(info.serialNumber) == safeStr(list.pDeviceInfoList[i].serialNumber))
                {
                    out = list.pDeviceInfoList[i];
                    return true;
                }
            }
        }

        if ((info.deviceId && info.deviceId[0]))
        {
            for (unsigned int i = 0; i < list.nDevNum; ++i)
            {
                if (safeStr(info.deviceId) == safeStr(list.pDeviceInfoList[i].cameraKey))
                {
                    out = list.pDeviceInfoList[i];
                    return true;
                }
            }
        }

        const int logicalIndex = tryParseLogicalCameraIndex(info.logicalName ? info.logicalName : "");
        if (logicalIndex >= 0 && static_cast<unsigned int>(logicalIndex) < list.nDevNum)
        {
            out = list.pDeviceInfoList[logicalIndex];
            return true;
        }
        return false;
    }

    bool isClInterfaceType(IMV_FG_EInterfaceType t)
    {
        return t == typeCLInterface;
    }

    /**
         * 把 IMVFG 像素格式枚举映射成统一的 PixelFormat。
         * 只识别业务最常用的几种；其余落到 Unknown，但帧仍照常下发。
         */
    PixelFormat mapPixelFormat(IMV_FG_EPixelType fmt, int &channelsOut)
    {
        switch (fmt)
        {
        case IMV_FG_PIXEL_TYPE_Mono8:
        case IMV_FG_PIXEL_TYPE_Mono8S:
        case IMV_FG_PIXEL_TYPE_BayGR8:
        case IMV_FG_PIXEL_TYPE_BayRG8:
        case IMV_FG_PIXEL_TYPE_BayGB8:
        case IMV_FG_PIXEL_TYPE_BayBG8:
            channelsOut = 1;
            return PixelFormat::Mono8;

        case IMV_FG_PIXEL_TYPE_RGB8:
            channelsOut = 3;
            return PixelFormat::RGB8;

        case IMV_FG_PIXEL_TYPE_BGR8:
            channelsOut = 3;
            return PixelFormat::BGR8;

        default:
            channelsOut = 0;
            return PixelFormat::Unknown;
        }
    }

    } // namespace

    // ============================================================
    // Factory
    // ============================================================

    std::vector<CameraInfo> HuarayCameraSdkFactory::enumerateDevices(std::string * errorMessage)
    {
        clearError(errorMessage);

        std::vector<CameraInfo> devices;

        IMV_FG_INTERFACE_INFO_LIST ifList{};
        int nRet = IMV_FG_EnumInterface(typeInterfaceAll, &ifList);
        if (nRet != IMV_FG_OK)
        {
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage("IMV_FG_EnumInterface", nRet);
            }
            LOGM_ERROR("Camera", "Huaray enumerateDevices: IMV_FG_EnumInterface failed, status={}", nRet);
            return devices;
        }

        if (ifList.nInterfaceNum == 0)
        {
            LOGM_WARN("Camera", "Huaray enumerateDevices: no capture card found");
            return devices;
        }

        IMV_FG_DEVICE_INFO_LIST devList{};
        nRet = IMV_FG_EnumDevices(typeInterfaceAll, &devList);
        if (nRet != IMV_FG_OK)
        {
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage("IMV_FG_EnumDevices", nRet);
            }
            LOGM_ERROR("Camera", "Huaray enumerateDevices: IMV_FG_EnumDevices failed, status={}", nRet);
            return devices;
        }

        for (unsigned int i = 0; i < devList.nDevNum; ++i)
        {
            const IMV_FG_DEVICE_INFO &d = devList.pDeviceInfoList[i];

            if (safeStr(d.serialNumber).empty())
            {
                continue;
            }

            CameraInfo cameraInfo;
            cameraInfo.deviceId     = persistHuarayString(d.cameraKey);
            cameraInfo.serialNumber = persistHuarayString(d.serialNumber);
            cameraInfo.logicalName  = persistHuarayString("camera_" + std::to_string(i));
            cameraInfo.vendor       = persistHuarayString(d.vendorName);
            cameraInfo.model        = persistHuarayString(d.modelName);
            cameraInfo.role         = CameraRole::Unknown;
            devices.push_back(std::move(cameraInfo));
        }

        return devices;
    }

    std::unique_ptr<ICameraSdkAdapter> HuarayCameraSdkFactory::createAdapter(const CameraInfo &info, std::string *errorMessage)
    {
        clearError(errorMessage);
        try
        {
            return std::make_unique<CameraHuaraySdkAdapter>(info);
        } catch (const std::exception &e)
        {
            if (errorMessage)
            {
                *errorMessage = e.what();
            }
            return nullptr;
        }
    }

    std::vector<std::unique_ptr<ICameraSdkAdapter>> HuarayCameraSdkFactory::createAllAdapters()
    {
        std::vector<std::unique_ptr<ICameraSdkAdapter>> adapters;

        std::string err;
        auto devices = enumerateDevices(&err);
        if (!err.empty())
        {
            LOGM_ERROR("Camera", "Huaray createAllAdapters: enumerateDevices failed: {}", err);
            return adapters;
        }
        if (devices.empty())
        {
            LOGM_WARN("Camera", "Huaray createAllAdapters: no devices found");
            return adapters;
        }

        adapters.reserve(devices.size());
        for (const auto &dev : devices)
        {
            std::string createErr;
            auto adapter = createAdapter(dev, &createErr);
            if (!adapter)
            {
                LOGM_ERROR("Camera", "Huaray createAdapter failed for device={} err={}", dev.deviceId, createErr);
                continue;
            }
            adapters.emplace_back(std::move(adapter));
        }
        return adapters;
    }

    // ============================================================
    // Adapter::Impl
    // ============================================================

    struct FrameEnvelope
    {
        FramePacket packet;
        std::vector<std::uint8_t> bytes;
    };

    struct CameraHuaraySdkAdapter::Impl
    {
        struct DeviceContext
        {
            IMV_FG_IF_HANDLE ifHandle{nullptr};
            IMV_FG_DEV_HANDLE devHandle{nullptr};
            IMV_FG_EInterfaceType interfaceType{typeUndefinedInterface};
            std::string cameraKey;
            std::string interfaceKey;
            std::atomic<std::uint64_t> seq_{1};

            void resetHandles()
            {
                ifHandle  = nullptr;
                devHandle = nullptr;
            }

            void resetAll()
            {
                resetHandles();
                interfaceType = typeUndefinedInterface;
                cameraKey.clear();
                interfaceKey.clear();
            }
        };

        int bufferCount{8};
        std::atomic_bool cb_installed{false};
        FrameCallback frameCallback{nullptr};
        std::atomic<CameraState> state{CameraState::Created};
        CameraHuaraySdkAdapter *owner{nullptr};
        DeviceContext ctx{};
        CameraConfig currentConfig{};
        CameraInfo boundInfo{};

        explicit Impl(CameraHuaraySdkAdapter *self, CameraInfo info) :
            owner(self),
            boundInfo(std::move(info))
        {
            state.store(CameraState::Created, std::memory_order_relaxed);
        }

        ~Impl()
        {
            cleanup();
        }

        // ----- open / close -----

        bool open(std::string *errorMessage)
        {
            clearError(errorMessage);

            IMV_FG_DEVICE_INFO devInfo{};
            if (!find_device(boundInfo, devInfo))
            {
                if (errorMessage)
                {
                    *errorMessage = "find_device failed (serial=" + safeStr(boundInfo.serialNumber) + ")";
                }
                return false;
            }

            ctx.cameraKey     = safeStr(devInfo.cameraKey);
            ctx.interfaceKey  = safeStr(devInfo.FGInterfaceInfo.interfaceKey);
            ctx.interfaceType = devInfo.FGInterfaceInfo.nInterfaceType;

            int nRet =
                IMV_FG_OpenInterfaceEx(IMV_FG_MODE_BY_CAMERAKEY, const_cast<char *>(ctx.interfaceKey.c_str()), &ctx.ifHandle);
            if (nRet != IMV_FG_OK || !ctx.ifHandle)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage("IMV_FG_OpenInterfaceEx", nRet);
                }
                ctx.resetAll();
                return false;
            }

            nRet = IMV_FG_OpenDevice(IMV_FG_MODE_BY_CAMERAKEY, const_cast<char *>(ctx.cameraKey.c_str()), &ctx.devHandle);
            if (nRet != IMV_FG_OK || !ctx.devHandle)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage("IMV_FG_OpenDevice", nRet);
                }
                IMV_FG_CloseInterface(ctx.ifHandle);
                ctx.resetAll();
                return false;
            }

            state.store(CameraState::Opened, std::memory_order_relaxed);
            return true;
        }

        void cleanup()
        {
            if (ctx.devHandle)
            {
                if (IMV_FG_IsGrabbing(ctx.devHandle))
                {
                    IMV_FG_StopGrabbing(ctx.devHandle);
                }
                IMV_FG_CloseDevice(ctx.devHandle);
                ctx.devHandle = nullptr;
            }
            if (ctx.ifHandle)
            {
                IMV_FG_CloseInterface(ctx.ifHandle);
                ctx.ifHandle = nullptr;
            }
            cb_installed = false;
            state.store(CameraState::Stopped, std::memory_order_relaxed);
        }

        // ----- helpers -----

        // 触发模式相关属性需要写在哪个句柄上：CL 写卡，其他写相机。
        HANDLE triggerOwnerHandle() const
        {
            if (isClInterfaceType(ctx.interfaceType))
            {
                return ctx.ifHandle;
            }
            return ctx.devHandle;
        }

        bool setEnumSymbol(HANDLE h, const char *feature, const char *value, const char *step, std::string *errorMessage)
        {
            const int nRet = IMV_FG_SetEnumFeatureSymbol(h, feature, value);
            if (nRet != IMV_FG_OK)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage(std::string(step) + ": " + feature + "=" + value, nRet);
                }
                return false;
            }
            return true;
        }

        bool setDouble(HANDLE h, const char *feature, double value, const char *step, std::string *errorMessage)
        {
            const int nRet = IMV_FG_SetDoubleFeatureValue(h, feature, value);
            if (nRet != IMV_FG_OK)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage(step, nRet);
                }
                return false;
            }
            return true;
        }

        // ----- trigger configuration -----

        bool configureTrigger(TriggerMode trigger, std::string *errorMessage)
        {
            if (!ctx.devHandle)
            {
                if (errorMessage)
                {
                    *errorMessage = "device handle is null";
                }
                return false;
            }

            const bool isCl = isClInterfaceType(ctx.interfaceType);

            switch (trigger)
            {
            case TriggerMode::Continuous:
                // 关闭帧触发即连续模式
                return setEnumSymbol(ctx.devHandle, "TriggerMode", "Off", "set TriggerMode", errorMessage);

            case TriggerMode::Software:
                if (isCl)
                {
                    // CL: 卡上 CC1=SofwareTrigger（SDK 文档原始拼写）
                    if (!setEnumSymbol(ctx.ifHandle, "CC1", "SofwareTrigger", "set IF.CC1", errorMessage))
                    {
                        return false;
                    }
                    if (!setEnumSymbol(ctx.devHandle, "TriggerSource", "CC1", "set TriggerSource", errorMessage))
                    {
                        return false;
                    }
                    if (!setEnumSymbol(ctx.devHandle, "LineSelector", "CC1", "set LineSelector", errorMessage))
                    {
                        return false;
                    }
                }
                else
                {
                    if (!setEnumSymbol(ctx.devHandle, "TriggerSelector", "FrameStart", "set TriggerSelector", errorMessage))
                    {
                        return false;
                    }
                    if (!setEnumSymbol(ctx.devHandle, "TriggerSource", "Software", "set TriggerSource", errorMessage))
                    {
                        return false;
                    }
                }
                return setEnumSymbol(ctx.devHandle, "TriggerMode", "On", "set TriggerMode", errorMessage);

            case TriggerMode::Hardware:
                if (isCl)
                {
                    if (!setEnumSymbol(ctx.ifHandle, "CC1", "ExternalTrigger1", "set IF.CC1", errorMessage))
                    {
                        return false;
                    }
                    if (!setEnumSymbol(ctx.devHandle, "TriggerSource", "CC1", "set TriggerSource", errorMessage))
                    {
                        return false;
                    }
                    if (!setEnumSymbol(ctx.devHandle, "LineSelector", "CC1", "set LineSelector", errorMessage))
                    {
                        return false;
                    }
                }
                else
                {
                    if (!setEnumSymbol(ctx.devHandle, "TriggerSelector", "FrameStart", "set TriggerSelector", errorMessage))
                    {
                        return false;
                    }
                    if (!setEnumSymbol(ctx.devHandle, "TriggerSource", "Line1", "set TriggerSource", errorMessage))
                    {
                        return false;
                    }
                }
                return setEnumSymbol(ctx.devHandle, "TriggerMode", "On", "set TriggerMode", errorMessage);

            default:
                if (errorMessage)
                {
                    *errorMessage = "unsupported trigger mode";
                }
                return false;
            }
        }

        // ----- streaming -----

        bool startStream(std::string *errorMessage)
        {
            clearError(errorMessage);

            if (!ctx.devHandle)
            {
                if (errorMessage)
                {
                    *errorMessage = "device is not opened";
                }
                return false;
            }

            // 缓存数量必须在 StartGrabbing 之前设置
            if (bufferCount >= 2)
            {
                const int nRet = IMV_FG_SetBufferCount(ctx.devHandle, static_cast<unsigned int>(bufferCount));
                if (nRet != IMV_FG_OK)
                {
                    LOGM_WARN("Camera", "Huaray IMV_FG_SetBufferCount failed, status={} (continue with default)", nRet);
                }
            }

            const int nReg = IMV_FG_AttachGrabbing(ctx.devHandle, &CameraHuaraySdkAdapter::Impl::on_frame, this);
            if (nReg != IMV_FG_OK)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage("IMV_FG_AttachGrabbing", nReg);
                }
                return false;
            }
            cb_installed = true;

            const int nSt = IMV_FG_StartGrabbing(ctx.devHandle);
            if (nSt != IMV_FG_OK)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage("IMV_FG_StartGrabbing", nSt);
                }
                return false;
            }

            state.store(CameraState::Streaming, std::memory_order_relaxed);
            return true;
        }

        bool stopStream(std::string *errorMessage)
        {
            clearError(errorMessage);

            if (ctx.devHandle && IMV_FG_IsGrabbing(ctx.devHandle))
            {
                const int nRet = IMV_FG_StopGrabbing(ctx.devHandle);
                if (nRet != IMV_FG_OK)
                {
                    if (errorMessage)
                    {
                        *errorMessage = makeErrorMessage("IMV_FG_StopGrabbing", nRet);
                    }
                    return false;
                }
            }

            cb_installed = false;
            state.store(CameraState::Stopped, std::memory_order_relaxed);
            return true;
        }

        bool softwareTrigger(std::string *errorMessage)
        {
            if (!ctx.devHandle)
            {
                if (errorMessage)
                {
                    *errorMessage = "device handle is null";
                }
                return false;
            }

            HANDLE target  = triggerOwnerHandle();
            const int nRet = IMV_FG_ExecuteCommandFeature(target, "TriggerSoftware");
            if (nRet != IMV_FG_OK)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage("TriggerSoftware", nRet);
                }
                return false;
            }
            return true;
        }

        // ----- frame callback -----

        static void on_frame(IMV_FG_Frame *pFrame, void *pUser)
        {
            if (!pFrame || !pUser)
            {
                return;
            }
            auto *self = static_cast<Impl *>(pUser);
            self->dispatch(*pFrame);
        }

        void dispatch(const IMV_FG_Frame &f)
        {
            if (!frameCallback)
            {
                return;
            }

            auto envelope = std::make_shared<FrameEnvelope>();
            if (!makeFrame(f, *envelope))
            {
                return;
            }
            auto shared = std::shared_ptr<const FramePacket>(envelope, &envelope->packet);
            frameCallback(shared);
        }

        bool makeFrame(const IMV_FG_Frame &src, FrameEnvelope &envelope)
        {
            auto &out      = envelope.packet;
            const auto &fi = src.frameInfo;
            if (!src.pData || fi.size == 0 || fi.width == 0 || fi.height == 0)
            {
                return false;
            }

            int channels         = 0;
            const PixelFormat pf = mapPixelFormat(fi.pixelFormat, channels);

            out                         = {};
            out.meta.cameraName         = boundInfo.logicalName;
            out.meta.cameraSerialNumber = boundInfo.serialNumber;
            out.meta.frameId            = ctx.seq_.fetch_add(1, std::memory_order_relaxed);
            out.meta.timestampMs        = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                    .count());

            out.width       = static_cast<int>(fi.width);
            out.height      = static_cast<int>(fi.height);
            out.channels    = channels;
            out.pixelFormat = pf;

            envelope.bytes.resize(fi.size);
            out.data     = envelope.bytes.data();
            out.dataSize = envelope.bytes.size();
            std::memcpy(envelope.bytes.data(), src.pData, fi.size);
            return true;
        }
    };

    // ============================================================
    // Adapter
    // ============================================================

    CameraHuaraySdkAdapter::CameraHuaraySdkAdapter(CameraInfo info) :
        info_(std::move(info)),
        impl_(std::make_unique<Impl>(this, info_))
    {
    }

    CameraHuaraySdkAdapter::~CameraHuaraySdkAdapter() = default;

    std::vector<CameraInfo> CameraHuaraySdkAdapter::enumerateDevices(std::string * errorMessage)
    {
        clearError(errorMessage);
        return {info_};
    }

    bool CameraHuaraySdkAdapter::open(std::string * errorMessage)
    {
        const auto state = impl_->state.load(std::memory_order_relaxed);
        if (state == CameraState::Opened || state == CameraState::Configured || state == CameraState::Streaming)
        {
            clearError(errorMessage);
            return true;
        }

        const bool ok = impl_->open(errorMessage);
        if (ok)
        {
            LOGM_INFO("Camera", "CameraHuaraySdkAdapter open success, logical_name={}, serial={}", info_.logicalName,
                      info_.serialNumber);
        }
        else
        {
            LOGM_ERROR("Camera", "CameraHuaraySdkAdapter open failed, logical_name={}, serial={}, err={}", info_.logicalName,
                       info_.serialNumber, errorMessage ? *errorMessage : std::string{});
        }
        return ok;
    }

    bool CameraHuaraySdkAdapter::close(std::string * errorMessage)
    {
        clearError(errorMessage);
        impl_->cleanup();
        LOGM_INFO("Camera", "CameraHuaraySdkAdapter close, logical_name={}", info_.logicalName);
        return true;
    }

    bool CameraHuaraySdkAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage)
    {
        clearError(errorMessage);

        const auto state      = impl_->state.load(std::memory_order_relaxed);
        const bool isIdleLike = state == CameraState::Opened || state == CameraState::Configured || state == CameraState::Stopped;
        const bool isStreaming = state == CameraState::Streaming;

        if (!isIdleLike && !isStreaming)
        {
            if (errorMessage)
            {
                *errorMessage = "camera state does not allow applyConfig";
            }
            LOGM_ERROR("Camera", "CameraHuaraySdkAdapter applyConfig rejected, state={}", static_cast<int>(state));
            return false;
        }

        // 运行中只允许改 exposure / gain
        if (isStreaming)
        {
            const bool wantsTriggerChange = config.triggerMode != impl_->currentConfig.triggerMode;
            const int newBufferCount      = config.bufferCount > 0 ? config.bufferCount : 8;
            const bool wantsBufferChange  = newBufferCount != impl_->bufferCount;
            if (wantsTriggerChange || wantsBufferChange)
            {
                if (errorMessage)
                {
                    *errorMessage = "triggerMode/bufferCount cannot be changed while streaming";
                }
                return false;
            }

            if (config.exposureTime > 0.0)
            {
                if (!impl_->setDouble(impl_->ctx.devHandle, "ExposureTime", config.exposureTime, "set ExposureTime",
                                      errorMessage))
                {
                    return false;
                }
                impl_->currentConfig.exposureTime = config.exposureTime;
            }
            if (config.gain > 0.0)
            {
                if (!impl_->setDouble(impl_->ctx.devHandle, "Gain", config.gain, "set Gain", errorMessage))
                {
                    return false;
                }
                impl_->currentConfig.gain = config.gain;
            }
            LOGM_INFO("Camera", "CameraHuaraySdkAdapter applyConfig (streaming) success, logical_name={}, exposure={}, gain={}",
                      info_.logicalName, config.exposureTime, config.gain);
            return true;
        }

        // 非 streaming：全量配置
        impl_->currentConfig = config;
        impl_->bufferCount   = config.bufferCount > 0 ? config.bufferCount : 8;

        if (!impl_->configureTrigger(config.triggerMode, errorMessage))
        {
            LOGM_ERROR("Camera", "CameraHuaraySdkAdapter configureTrigger failed, logical_name={}, value={}", info_.logicalName,
                       static_cast<int>(config.triggerMode));
            return false;
        }

        if (config.exposureTime > 0.0)
        {
            if (!impl_->setDouble(impl_->ctx.devHandle, "ExposureTime", config.exposureTime, "set ExposureTime", errorMessage))
            {
                return false;
            }
        }
        if (config.gain > 0.0)
        {
            if (!impl_->setDouble(impl_->ctx.devHandle, "Gain", config.gain, "set Gain", errorMessage))
            {
                return false;
            }
        }

        impl_->state.store(CameraState::Configured, std::memory_order_relaxed);
        LOGM_INFO("Camera", "CameraHuaraySdkAdapter applyConfig success, logical_name={}", info_.logicalName);
        return true;
    }

    bool CameraHuaraySdkAdapter::startGrabbing(std::string * errorMessage)
    {
        return impl_->startStream(errorMessage);
    }

    bool CameraHuaraySdkAdapter::stopGrabbing(std::string * errorMessage)
    {
        return impl_->stopStream(errorMessage);
    }

    bool CameraHuaraySdkAdapter::executeSoftwareTrigger(std::string * errorMessage)
    {
        return impl_->softwareTrigger(errorMessage);
    }

    void CameraHuaraySdkAdapter::setFrameCallback(FrameCallback callback)
    {
        impl_->frameCallback = std::move(callback);
    }

    CameraInfo CameraHuaraySdkAdapter::info() const
    {
        return info_;
    }
}
