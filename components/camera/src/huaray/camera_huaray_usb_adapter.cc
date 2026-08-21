#include "huaray/camera_huaray_usb_adapter.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "Logger.h"

namespace SZXCamera
{

namespace
{

thread_local const void *g_huarayUsbCallbackImpl = nullptr;

void clearError(std::string *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
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
         * 通过 IMV_EnumDevices 找一台相机。
         *
         * 匹配优先级：serialNumber → deviceId(cameraKey) → logicalName "camera_N"
         */
bool find_device(const CameraInfo &info, IMV_DeviceInfo &out)
{
    IMV_DeviceList list{};
    if (IMV_EnumDevices(&list, interfaceTypeAll) != IMV_OK)
    {
        return false;
    }
    if (list.nDevNum == 0 || list.pDevInfo == nullptr)
    {
        return false;
    }

    if ((info.serialNumber && info.serialNumber[0]))
    {
        for (unsigned int i = 0; i < list.nDevNum; ++i)
        {
            if (safeStr(info.serialNumber) == safeStr(list.pDevInfo[i].serialNumber))
            {
                out = list.pDevInfo[i];
                return true;
            }
        }
    }
    if ((info.deviceId && info.deviceId[0]))
    {
        for (unsigned int i = 0; i < list.nDevNum; ++i)
        {
            if (safeStr(info.deviceId) == safeStr(list.pDevInfo[i].cameraKey))
            {
                out = list.pDevInfo[i];
                return true;
            }
        }
    }
    const int logicalIndex = tryParseLogicalCameraIndex(info.logicalName ? info.logicalName : "");
    if (logicalIndex >= 0 && static_cast<unsigned int>(logicalIndex) < list.nDevNum)
    {
        out = list.pDevInfo[logicalIndex];
        return true;
    }
    return false;
}

/**
         * 把 IMV 像素格式映射成统一 PixelFormat。
         * 只识别业务最常用的几种；其余落到 Unknown，但帧仍照常下发。
         */
PixelFormat mapPixelFormat(IMV_EPixelType fmt, int &channelsOut)
{
    switch (fmt)
    {
    case gvspPixelMono8:
    case gvspPixelMono8S:
    case gvspPixelBayGR8:
    case gvspPixelBayRG8:
    case gvspPixelBayGB8:
    case gvspPixelBayBG8:
        channelsOut = 1;
        return PixelFormat::Mono8;

    case gvspPixelRGB8:
        channelsOut = 3;
        return PixelFormat::RGB8;

    case gvspPixelBGR8:
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

std::vector<CameraInfo> HuarayUsbCameraSdkFactory::enumerateDevices(std::string *errorMessage)
{
    clearError(errorMessage);

    std::vector<CameraInfo> devices;

    IMV_DeviceList list{};
    const int nRet = IMV_EnumDevices(&list, interfaceTypeAll);
    if (nRet != IMV_OK)
    {
        if (errorMessage)
        {
            *errorMessage = makeErrorMessage("IMV_EnumDevices", nRet);
        }
        LOGM_ERROR("Camera", "HuarayUsb enumerateDevices: IMV_EnumDevices failed, status={}", nRet);
        return devices;
    }

    if (list.nDevNum == 0 || list.pDevInfo == nullptr)
    {
        LOGM_WARN("Camera", "HuarayUsb enumerateDevices: no direct-connect cameras found");
        return devices;
    }

    for (unsigned int i = 0; i < list.nDevNum; ++i)
    {
        const IMV_DeviceInfo &d = list.pDevInfo[i];

        if (safeStr(d.serialNumber).empty())
        {
            continue;
        }

        CameraInfo cameraInfo;
        deviceIds_.push_back(safeStr(d.cameraKey));
        serialNumbers_.push_back(safeStr(d.serialNumber));
        logicalNames_.push_back("camera_" + std::to_string(i));
        vendors_.push_back(safeStr(d.vendorName));
        models_.push_back(safeStr(d.modelName));
        cameraInfo.deviceId     = deviceIds_.back().c_str();
        cameraInfo.serialNumber = serialNumbers_.back().c_str();
        cameraInfo.logicalName  = logicalNames_.back().c_str();
        cameraInfo.vendor       = vendors_.back().c_str();
        cameraInfo.model        = models_.back().c_str();
        cameraInfo.role         = CameraRole::Unknown;
        devices.push_back(std::move(cameraInfo));
    }

    return devices;
}

std::unique_ptr<ICameraSdkAdapter> HuarayUsbCameraSdkFactory::createAdapter(const CameraInfo &info, std::string *errorMessage)
{
    clearError(errorMessage);
    try
    {
        return std::make_unique<CameraHuarayUsbSdkAdapter>(info);
    } catch (const std::exception &e)
    {
        if (errorMessage)
        {
            *errorMessage = e.what();
        }
        return nullptr;
    }
}

std::vector<std::unique_ptr<ICameraSdkAdapter>> HuarayUsbCameraSdkFactory::createAllAdapters()
{
    std::vector<std::unique_ptr<ICameraSdkAdapter>> adapters;
    std::string err;
    auto devices = enumerateDevices(&err);
    if (!err.empty())
    {
        LOGM_ERROR("Camera", "HuarayUsb createAllAdapters: enumerateDevices failed: {}", err);
        return adapters;
    }
    if (devices.empty())
    {
        LOGM_WARN("Camera", "HuarayUsb createAllAdapters: no devices found");
        return adapters;
    }
    adapters.reserve(devices.size());
    for (const auto &dev : devices)
    {
        std::string createErr;
        auto adapter = createAdapter(dev, &createErr);
        if (!adapter)
        {
            LOGM_ERROR("Camera", "HuarayUsb createAdapter failed for device={} err={}", dev.deviceId, createErr);
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

struct CameraHuarayUsbSdkAdapter::Impl
{
    struct DeviceContext
    {
        IMV_HANDLE handle{nullptr};
        std::string cameraKey;
        std::atomic<std::uint64_t> seq_{1};

        void resetAll()
        {
            handle = nullptr;
            cameraKey.clear();
        }
    };

    int bufferCount{8};
    std::atomic_bool cb_installed{false};
    FrameCallback frameCallback{nullptr};
    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::size_t activeCallbacks{0};
    bool callbacksEnabled{false};
    std::atomic<CameraState> state{CameraState::Created};
    CameraHuarayUsbSdkAdapter *owner{nullptr};
    DeviceContext ctx{};
    CameraConfig currentConfig{};
    CameraInfo boundInfo{};

    explicit Impl(CameraHuarayUsbSdkAdapter *self, CameraInfo info) :
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

        IMV_DeviceInfo devInfo{};
        if (!find_device(boundInfo, devInfo))
        {
            if (errorMessage)
            {
                *errorMessage = "find_device failed (serial=" + safeStr(boundInfo.serialNumber) + ")";
            }
            return false;
        }

        ctx.cameraKey = safeStr(devInfo.cameraKey);

        int nRet = IMV_CreateHandle(&ctx.handle, modeByCameraKey, const_cast<char *>(ctx.cameraKey.c_str()));
        if (nRet != IMV_OK || !ctx.handle)
        {
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage("IMV_CreateHandle", nRet);
            }
            ctx.resetAll();
            return false;
        }

        nRet = IMV_Open(ctx.handle);
        if (nRet != IMV_OK)
        {
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage("IMV_Open", nRet);
            }
            IMV_DestroyHandle(ctx.handle);
            ctx.resetAll();
            return false;
        }

        state.store(CameraState::Opened, std::memory_order_relaxed);
        return true;
    }

    void cleanup()
    {
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callbacksEnabled = false;
        }
        if (ctx.handle)
        {
            if (IMV_IsGrabbing(ctx.handle))
            {
                IMV_StopGrabbing(ctx.handle);
            }
            if (IMV_IsOpen(ctx.handle))
            {
                IMV_Close(ctx.handle);
            }
            IMV_DestroyHandle(ctx.handle);
            ctx.handle = nullptr;
        }
        {
            std::unique_lock<std::mutex> lock(callbackMutex);
            callbackCv.wait(lock, [this] { return activeCallbacks == (g_huarayUsbCallbackImpl == this ? 1u : 0u); });
        }
        cb_installed = false;
        state.store(CameraState::Stopped, std::memory_order_relaxed);
    }

    // ----- helpers -----

    bool setEnumSymbol(const char *feature, const char *value, const char *step, std::string *errorMessage)
    {
        const int nRet = IMV_SetEnumFeatureSymbol(ctx.handle, feature, value);
        if (nRet != IMV_OK)
        {
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage(std::string(step) + ": " + feature + "=" + value, nRet);
            }
            return false;
        }
        return true;
    }

    bool setDouble(const char *feature, double value, const char *step, std::string *errorMessage)
    {
        const int nRet = IMV_SetDoubleFeatureValue(ctx.handle, feature, value);
        if (nRet != IMV_OK)
        {
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage(step, nRet);
            }
            return false;
        }
        return true;
    }

    // 自动按属性类型 dispatch：有些相机的 Gain / ExposureTime 是 Int 不是 Double。
    // 如果属性不存在（IMV_FeatureIsAvailable=false），跳过且只打 warning，不算失败。
    bool setNumeric(const char *feature, double value, const char *step, std::string *errorMessage)
    {
        if (!IMV_FeatureIsAvailable(ctx.handle, feature))
        {
            LOGM_WARN("Camera", "HuarayUsb feature not available, skip: {}", feature);
            return true;
        }

        IMV_EFeatureType type = featureUndefined;
        if (!IMV_GetFeatureType(ctx.handle, feature, &type))
        {
            if (errorMessage)
            {
                *errorMessage = std::string(step) + ": IMV_GetFeatureType failed for " + feature;
            }
            return false;
        }

        int nRet = IMV_OK;
        switch (type)
        {
        case featureFloat:
            nRet = IMV_SetDoubleFeatureValue(ctx.handle, feature, value);
            break;
        case featureInt:
            nRet = IMV_SetIntFeatureValue(ctx.handle, feature, static_cast<int64_t>(value));
            break;
        default:
            if (errorMessage)
            {
                std::ostringstream oss;
                oss << step << ": unsupported feature type " << static_cast<int>(type) << " for " << feature;
                *errorMessage = oss.str();
            }
            return false;
        }

        if (nRet != IMV_OK)
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
        if (!ctx.handle)
        {
            if (errorMessage)
            {
                *errorMessage = "device handle is null";
            }
            return false;
        }

        switch (trigger)
        {
        case TriggerMode::Continuous:
            return setEnumSymbol("TriggerMode", "Off", "set TriggerMode", errorMessage);

        case TriggerMode::Software:
            if (!setEnumSymbol("TriggerSelector", "FrameStart", "set TriggerSelector", errorMessage))
                return false;
            if (!setEnumSymbol("TriggerSource", "Software", "set TriggerSource", errorMessage))
                return false;
            return setEnumSymbol("TriggerMode", "On", "set TriggerMode", errorMessage);

        case TriggerMode::Hardware:
            if (!setEnumSymbol("TriggerSelector", "FrameStart", "set TriggerSelector", errorMessage))
                return false;
            if (!setEnumSymbol("TriggerSource", "Line1", "set TriggerSource", errorMessage))
                return false;
            return setEnumSymbol("TriggerMode", "On", "set TriggerMode", errorMessage);

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
        if (!ctx.handle)
        {
            if (errorMessage)
            {
                *errorMessage = "device is not opened";
            }
            return false;
        }

        if (bufferCount >= 2)
        {
            const int nRet = IMV_SetBufferCount(ctx.handle, static_cast<unsigned int>(bufferCount));
            if (nRet != IMV_OK)
            {
                LOGM_WARN("Camera", "HuarayUsb IMV_SetBufferCount failed, status={} (continue with default)", nRet);
            }
        }

        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callbacksEnabled = true;
        }

        const int nReg = IMV_AttachGrabbing(ctx.handle, &CameraHuarayUsbSdkAdapter::Impl::on_frame, this);
        if (nReg != IMV_OK)
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callbacksEnabled = false;
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage("IMV_AttachGrabbing", nReg);
            }
            return false;
        }
        cb_installed = true;

        const int nSt = IMV_StartGrabbing(ctx.handle);
        if (nSt != IMV_OK)
        {
            IMV_StopGrabbing(ctx.handle);
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                callbacksEnabled = false;
            }
            cb_installed = false;
            if (errorMessage)
            {
                *errorMessage = makeErrorMessage("IMV_StartGrabbing", nSt);
            }
            return false;
        }

        state.store(CameraState::Streaming, std::memory_order_relaxed);
        return true;
    }

    bool stopStream(std::string *errorMessage)
    {
        clearError(errorMessage);
        if (ctx.handle && IMV_IsGrabbing(ctx.handle))
        {
            const int nRet = IMV_StopGrabbing(ctx.handle);
            if (nRet != IMV_OK)
            {
                if (errorMessage)
                {
                    *errorMessage = makeErrorMessage("IMV_StopGrabbing", nRet);
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
        if (!ctx.handle)
        {
            if (errorMessage)
            {
                *errorMessage = "device handle is null";
            }
            return false;
        }
        const int nRet = IMV_ExecuteCommandFeature(ctx.handle, "TriggerSoftware");
        if (nRet != IMV_OK)
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

    static void on_frame(IMV_Frame *pFrame, void *pUser)
    {
        if (!pFrame || !pUser)
            return;
        auto *self = static_cast<Impl *>(pUser);
        self->dispatch(*pFrame);
    }

    void dispatch(const IMV_Frame &f)
    {
        FrameCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (!callbacksEnabled)
                return;
            callback = frameCallback;
            if (callback)
                ++activeCallbacks;
        }
        if (!callback)
            return;
        struct CallbackGuard
        {
            Impl *impl;
            ~CallbackGuard()
            {
                std::lock_guard<std::mutex> lock(impl->callbackMutex);
                if (--impl->activeCallbacks == 0)
                    impl->callbackCv.notify_all();
                g_huarayUsbCallbackImpl = nullptr;
            }
        } guard{this};
        g_huarayUsbCallbackImpl = this;
        auto envelope           = std::make_shared<FrameEnvelope>();
        if (!makeFrame(f, *envelope))
            return;
        auto shared = std::shared_ptr<const FramePacket>(envelope, &envelope->packet);
        callback(shared);
    }

    bool makeFrame(const IMV_Frame &src, FrameEnvelope &envelope)
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
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

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

CameraHuarayUsbSdkAdapter::CameraHuarayUsbSdkAdapter(CameraInfo info) :
    info_(std::move(info)),
    impl_(std::make_unique<Impl>(this, info_))
{
}

CameraHuarayUsbSdkAdapter::~CameraHuarayUsbSdkAdapter() = default;

std::vector<CameraInfo> CameraHuarayUsbSdkAdapter::enumerateDevices(std::string *errorMessage)
{
    clearError(errorMessage);
    return {info_};
}

bool CameraHuarayUsbSdkAdapter::open(std::string *errorMessage)
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
        LOGM_INFO("Camera", "CameraHuarayUsbSdkAdapter open success, logical_name={}, serial={}", info_.logicalName,
                  info_.serialNumber);
    }
    else
    {
        LOGM_ERROR("Camera", "CameraHuarayUsbSdkAdapter open failed, logical_name={}, serial={}, err={}", info_.logicalName,
                   info_.serialNumber, errorMessage ? *errorMessage : std::string{});
    }
    return ok;
}

bool CameraHuarayUsbSdkAdapter::close(std::string *errorMessage)
{
    clearError(errorMessage);
    impl_->cleanup();
    LOGM_INFO("Camera", "CameraHuarayUsbSdkAdapter close, logical_name={}", info_.logicalName);
    return true;
}

bool CameraHuarayUsbSdkAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage)
{
    clearError(errorMessage);

    const auto state       = impl_->state.load(std::memory_order_relaxed);
    const bool isIdleLike  = state == CameraState::Opened || state == CameraState::Configured || state == CameraState::Stopped;
    const bool isStreaming = state == CameraState::Streaming;

    if (!isIdleLike && !isStreaming)
    {
        if (errorMessage)
        {
            *errorMessage = "camera state does not allow applyConfig";
        }
        return false;
    }

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
            if (!impl_->setNumeric("ExposureTime", config.exposureTime, "set ExposureTime", errorMessage))
            {
                return false;
            }
            impl_->currentConfig.exposureTime = config.exposureTime;
        }
        if (config.gain > 0.0)
        {
            if (!impl_->setNumeric("Gain", config.gain, "set Gain", errorMessage))
            {
                return false;
            }
            impl_->currentConfig.gain = config.gain;
        }
        return true;
    }

    impl_->currentConfig = config;
    impl_->bufferCount   = config.bufferCount > 0 ? config.bufferCount : 8;

    if (!impl_->configureTrigger(config.triggerMode, errorMessage))
    {
        return false;
    }
    if (config.exposureTime > 0.0)
    {
        if (!impl_->setNumeric("ExposureTime", config.exposureTime, "set ExposureTime", errorMessage))
        {
            return false;
        }
    }
    if (config.gain > 0.0)
    {
        if (!impl_->setNumeric("Gain", config.gain, "set Gain", errorMessage))
        {
            return false;
        }
    }

    impl_->state.store(CameraState::Configured, std::memory_order_relaxed);
    LOGM_INFO("Camera", "CameraHuarayUsbSdkAdapter applyConfig success, logical_name={}", info_.logicalName);
    return true;
}

bool CameraHuarayUsbSdkAdapter::startGrabbing(std::string *errorMessage)
{
    return impl_->startStream(errorMessage);
}

bool CameraHuarayUsbSdkAdapter::stopGrabbing(std::string *errorMessage)
{
    return impl_->stopStream(errorMessage);
}

bool CameraHuarayUsbSdkAdapter::executeSoftwareTrigger(std::string *errorMessage)
{
    return impl_->softwareTrigger(errorMessage);
}

void CameraHuarayUsbSdkAdapter::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(impl_->callbackMutex);
    impl_->frameCallback = std::move(callback);
}

CameraInfo CameraHuarayUsbSdkAdapter::info() const
{
    return info_;
}

} // namespace SZXCamera
