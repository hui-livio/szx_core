/**
 * @file camera_ikapsdk_adapter.cc
 * @brief IKap 相机适配器实现 — 封装枚举、采集、参数读写和回调分发。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "camera_ikapsdk_adapter.h"

#include "Logger.h"
#include "TimeHelper.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#ifndef INFINITE
    #define INFINITE 0xFFFFFFFF
#endif

using namespace SZX::LOGGER;
using namespace SZX::COMMON;

namespace
{
constexpr const char LOG_TAG[] = "Camera";

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

inline int logicalIndexFromName(const std::string &name)
{
    constexpr const char *prefix = "camera_";
    if (name.rfind(prefix, 0) != 0)
        return -1;
    try
    {
        return std::stoi(name.substr(std::strlen(prefix)));
    } catch (...)
    {
        return -1;
    }
}
} // namespace

namespace SZXCamera
{

// ========================================================================
// CameraIkapSdkAdapter::Impl — 内部实现（隔离 IKap SDK 依赖）
// ========================================================================

struct CameraIkapSdkAdapter::Impl
{
    // ---- 内部设备上下文 ----
    struct DeviceContext
    {
        std::uint32_t deviceIndex{std::numeric_limits<std::uint32_t>::max()};
        ITKDEVICE cameraHandle{nullptr};
        ITKSTREAM streamHandle{nullptr};
        ITKBUFFER convertBuffer{nullptr};
        ITKDEV_INFO deviceInfo{};
        std::uint32_t grabCount{ITKSTREAM_CONTINUOUS};
        std::atomic<std::uint64_t> seq_{1};

        void resetHandles()
        {
            cameraHandle  = nullptr;
            streamHandle  = nullptr;
            convertBuffer = nullptr;
        }
        void resetStreamHandle()
        {
            streamHandle = nullptr;
        }
        void resetCameraHandle()
        {
            cameraHandle = nullptr;
        }
        void resetConvertBuffer()
        {
            convertBuffer = nullptr;
        }
        void resetDeviceBinding()
        {
            deviceIndex = std::numeric_limits<std::uint32_t>::max();
            deviceInfo  = {};
        }
        void resetAll()
        {
            resetHandles();
            resetDeviceBinding();
            grabCount = ITKSTREAM_CONTINUOUS;
        }

        static bool find_device(const CameraInfo &info, std::uint32_t &idx, ITKDEV_INFO &out)
        {
            std::uint32_t cnt = 0;
            if (ItkManGetDeviceCount(&cnt) != ITKSTATUS_OK || cnt == 0)
                return false;

            if (info.serialNumber && info.serialNumber[0])
            {
                for (std::uint32_t i = 0; i < cnt; ++i)
                {
                    ITKDEV_INFO t{};
                    if (ItkManGetDeviceInfo(i, &t) != ITKSTATUS_OK)
                        continue;
                    if (info.serialNumber == std::string(t.SerialNumber))
                    {
                        idx = i;
                        out = t;
                        return true;
                    }
                }
            }

            if (info.deviceId && info.deviceId[0])
            {
                for (std::uint32_t i = 0; i < cnt; ++i)
                {
                    ITKDEV_INFO t{};
                    if (ItkManGetDeviceInfo(i, &t) != ITKSTATUS_OK)
                        continue;
                    if (info.deviceId == std::string(t.FullName))
                    {
                        idx = i;
                        out = t;
                        return true;
                    }
                }
            }

            const int logicalIndex = logicalIndexFromName(info.logicalName);
            if (logicalIndex >= 0 && static_cast<std::uint32_t>(logicalIndex) < cnt)
            {
                ITKDEV_INFO t{};
                if (ItkManGetDeviceInfo(static_cast<std::uint32_t>(logicalIndex), &t) == ITKSTATUS_OK)
                {
                    idx = static_cast<std::uint32_t>(logicalIndex);
                    out = t;
                    return true;
                }
            }

            return false;
        }
    };

    // ---- 成员 ----
    bool runtime_acquired{false};
    int bufferCount{72};
    std::atomic_bool cb_installed{false};
    FrameCallback frameCallback{nullptr};
    std::atomic<CameraState> state{CameraState::Created};
    CameraIkapSdkAdapter *owner{nullptr};
    DeviceContext ctx{};
    CameraConfig currentConfig{};
    CameraInfo boundInfo{};

    explicit Impl(CameraIkapSdkAdapter *self, CameraInfo info) :
        boundInfo(std::move(info)),
        owner(self)
    {
        state.store(CameraState::Created, std::memory_order_relaxed);
    }

    ~Impl()
    {
        cleanup();
        release_runtime();
    }

    // ---- 生命周期 ----

    bool ensure_runtime(std::string *errorMessage)
    {
        if (runtime_acquired)
        {
            clearError(errorMessage);
            return true;
        }
        if (!IkapSDKRuntime::instance().acquire(errorMessage))
            return false;
        runtime_acquired = true;
        return true;
    }

    void release_runtime()
    {
        if (!runtime_acquired)
            return;
        IkapSDKRuntime::instance().release();
        runtime_acquired = false;
    }

    // ---- 打开/关闭 ----

    bool open(std::string *errorMessage)
    {
        clearError(errorMessage);
        if (!ensure_runtime(errorMessage))
            return false;

        if (!ctx.find_device(boundInfo, ctx.deviceIndex, ctx.deviceInfo))
        {
            if (errorMessage)
                *errorMessage = "find_device failed";
            return false;
        }

        const auto status = ItkDevOpen(ctx.deviceIndex, ITKDEV_VAL_ACCESS_MODE_EXCLUSIVE, &ctx.cameraHandle);
        if (status != ITKSTATUS_OK || !ctx.cameraHandle)
        {
            if (errorMessage)
                *errorMessage = makeErrorMessage("ItkDevOpen failed", status);
            return false;
        }

        state.store(CameraState::Opened, std::memory_order_relaxed);
        clearError(errorMessage);
        return true;
    }

    void cleanup()
    {
        release_stream_resources();
        if (ctx.cameraHandle)
        {
            ItkDevClose(ctx.cameraHandle);
            ctx.resetCameraHandle();
        }
        state.store(CameraState::Stopped, std::memory_order_relaxed);
    }

    // ---- 流操作 ----

    bool start_stream(std::string *errorMessage)
    {
        clearError(errorMessage);
        if (!ctx.cameraHandle)
        {
            if (errorMessage)
                *errorMessage = "device is not opened";
            return false;
        }
        if (!create_stream_and_cvtbuf(errorMessage))
            return false;
        if (!config_stream(errorMessage))
            return false;

        const auto reg = ItkStreamRegisterCallback(ctx.streamHandle, ITKSTREAM_VAL_EVENT_TYPE_END_OF_FRAME, on_eof, this);
        if (reg != ITKSTATUS_OK)
        {
            if (errorMessage)
                *errorMessage = makeErrorMessage("ItkStreamRegisterCallback failed", reg);
            return false;
        }
        cb_installed = true;

        const auto st = ItkStreamStart(ctx.streamHandle, ITKSTREAM_CONTINUOUS);
        if (st != ITKSTATUS_OK)
        {
            if (errorMessage)
                *errorMessage = makeErrorMessage("ItkStreamStart failed", st);
            return false;
        }

        state.store(CameraState::Streaming, std::memory_order_relaxed);
        clearError(errorMessage);
        return true;
    }

    bool stop_stream(std::string *errorMessage)
    {
        clearError(errorMessage);
        release_stream_resources();
        state.store(CameraState::Stopped, std::memory_order_relaxed);
        return true;
    }

    bool create_stream_and_cvtbuf(std::string *errorMessage)
    {
        if (!ctx.cameraHandle)
        {
            if (errorMessage)
                *errorMessage = "device handle is null";
            return false;
        }

        std::uint32_t streamCount = 0;
        if (ItkDevGetStreamCount(ctx.cameraHandle, &streamCount) != ITKSTATUS_OK || streamCount == 0)
        {
            if (errorMessage)
                *errorMessage = "device has no stream channel";
            return false;
        }

        if (ItkDevAllocStreamEx(ctx.cameraHandle, 0, bufferCount, &ctx.streamHandle) != ITKSTATUS_OK || !ctx.streamHandle)
        {
            if (errorMessage)
                *errorMessage = "ItkDevAllocStreamEx failed";
            return false;
        }

        ITKBUFFER hBuf0 = nullptr;
        if (ItkStreamGetBuffer(ctx.streamHandle, 0, &hBuf0) != ITKSTATUS_OK || !hBuf0)
        {
            if (errorMessage)
                *errorMessage = "ItkStreamGetBuffer(0) failed";
            return false;
        }

        ITK_BUFFER_INFO bi{};
        if (ItkBufferGetInfo(hBuf0, &bi) != ITKSTATUS_OK)
        {
            if (errorMessage)
                *errorMessage = "ItkBufferGetInfo failed";
            return false;
        }

        if (!ctx.convertBuffer)
        {
            if (ItkBufferNew(bi.ImageWidth, bi.ImageHeight, ITKBUFFER_VAL_FORMAT_BGR161616, &ctx.convertBuffer) != ITKSTATUS_OK ||
                !ctx.convertBuffer)
            {
                if (errorMessage)
                    *errorMessage = "ItkBufferNew failed";
                return false;
            }
        }

        clearError(errorMessage);
        return true;
    }

    bool config_stream(std::string *errorMessage)
    {
        if (!ctx.streamHandle)
        {
            if (errorMessage)
                *errorMessage = "stream handle is null";
            return false;
        }

        const std::uint32_t startMode = ITKSTREAM_VAL_START_MODE_NON_BLOCK;
        const std::uint32_t xferMode  = ITKSTREAM_VAL_TRANSFER_MODE_SYNCHRONOUS_WITH_PROTECT;
        const std::uint32_t timeout   = INFINITE;
        const std::uint32_t autoClear = 0;
        // GigE Vision 包间隔超时：驱动等多久才判定一帧收完（与 Python 端对齐）
        const std::uint32_t gvPktTimeout = 500'000; // 500 ms

        const ITKSTATUS s1 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_START_MODE, &startMode);
        const ITKSTATUS s2 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_TRANSFER_MODE, &xferMode);
        const ITKSTATUS s3 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_TIME_OUT, &timeout);
        const ITKSTATUS s4 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_AUTO_CLEAR, &autoClear);
        const ITKSTATUS s5 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_GV_PACKET_INTER_TIMEOUT, &gvPktTimeout);

        if (!(s1 == ITKSTATUS_OK && s2 == ITKSTATUS_OK && s3 == ITKSTATUS_OK && s4 == ITKSTATUS_OK && s5 == ITKSTATUS_OK))
        {
            if (errorMessage)
                *errorMessage = "config_stream failed";
            return false;
        }

        clearError(errorMessage);
        return true;
    }

    void release_stream_resources()
    {
        if (ctx.streamHandle)
        {
            ItkStreamStop(ctx.streamHandle);
            if (cb_installed)
            {
                ItkStreamUnregisterCallback(ctx.streamHandle, ITKSTREAM_VAL_EVENT_TYPE_END_OF_FRAME);
                cb_installed = false;
            }
            ItkDevFreeStream(ctx.streamHandle);
            ctx.resetStreamHandle();
        }
        if (ctx.convertBuffer)
        {
            ItkBufferFree(ctx.convertBuffer);
            ctx.resetConvertBuffer();
        }
    }

    // ---- 帧回调 ----

    static void IKAPC_CC on_eof(std::uint32_t /*eventType*/, void *ctx)
    {
        auto *self = static_cast<Impl *>(ctx);
        if (!self || !self->frameCallback || !self->ctx.streamHandle)
            return;

        ITKBUFFER buf         = nullptr;
        std::uint32_t bStatus = 0;

        ItkStreamGetPrm(self->ctx.streamHandle, ITKSTREAM_PRM_CURRENT_BUFFER_HANDLE, &buf);
        if (!buf)
            return;

        ItkBufferGetPrm(buf, ITKBUFFER_PRM_STATE, &bStatus);
        if (bStatus == ITKBUFFER_VAL_STATE_FULL)
            self->dispatch(buf);

        ItkStreamClearBuffer(self->ctx.streamHandle, buf);
    }

    void dispatch(ITKBUFFER buf)
    {
        FramePacket frame;
        if (!make_frame(buf, frame))
            return;

        // 单次分配：FramePacket + 数据，通过别名 shared_ptr 绑定生命周期
        struct FrameBlk
        {
            FramePacket frame;
            std::uint8_t data[1];
        };
        const auto total = sizeof(FrameBlk) - 1 + frame.dataSize;
        auto *raw        = static_cast<FrameBlk *>(::operator new(total));
        auto blk         = std::shared_ptr<FrameBlk>(raw, [](FrameBlk *p) { ::operator delete(p); });

        blk->frame = frame;
        std::memcpy(blk->data, frame.data, frame.dataSize);
        blk->frame.data = blk->data;

        frameCallback(std::shared_ptr<const FramePacket>(blk, &blk->frame));
    }

    bool make_frame(ITKBUFFER buf, FramePacket &f)
    {
        if (!buf)
            return false;

        std::uint32_t stateVal = 0;
        ItkBufferGetPrm(buf, ITKBUFFER_PRM_STATE, &stateVal);
        if (stateVal != ITKBUFFER_VAL_STATE_FULL)
            return false;

        std::uint8_t need = 0;
        ItkBufferNeedAutoConvert(buf, &need);

        ITKBUFFER rd = buf;
        if (need)
        {
            if (!ctx.convertBuffer)
                return false;
            if (ItkBufferConvert(buf, ctx.convertBuffer, 0, ITKBUFFER_VAL_CONVERT_OPTION_AUTO_FORMAT) != ITKSTATUS_OK)
                return false;
            rd = ctx.convertBuffer;
        }

        int depth = 0, bits = 0;
        std::int64_t w = 0, h = 0;
        void *addr        = nullptr;
        std::uint64_t bid = 0;

        ItkBufferGetPrm(rd, ITKBUFFER_PRM_PIXEL_DEPTH, &depth);
        ItkBufferGetPrm(rd, ITKBUFFER_PRM_DATA_BIT, &bits);
        ItkBufferGetPrm(rd, ITKBUFFER_PRM_WIDTH, &w);
        ItkBufferGetPrm(rd, ITKBUFFER_PRM_HEIGHT, &h);
        ItkBufferGetPrm(rd, ITKBUFFER_PRM_ADDRESS, &addr);
        ItkBufferGetPrm(rd, ITKBUFFER_PRM_BLOCK_ID, &bid);

        if (!addr || w <= 0 || h <= 0)
            return false;

        const int bytesPerCh = (depth == 8 ? 1 : 2);
        const int denom      = (depth == 8 ? 8 : 16);
        const int ch         = denom ? (bits / denom) : 0;

        const std::uint32_t width  = static_cast<std::uint32_t>(w);
        const std::uint32_t height = static_cast<std::uint32_t>(h);
        const std::size_t nbytes   = width * static_cast<std::uint32_t>(ch) * static_cast<std::uint32_t>(bytesPerCh) * height;

        f                         = {};
        f.meta.cameraName         = boundInfo.logicalName;
        f.meta.cameraSerialNumber = boundInfo.serialNumber;
        // FramePacket::Meta is shared across the camera DLL boundary.  Do not
        // add an SDK-specific field here: use the existing source-order ID.
        // BLOCK_ID is the authoritative acquisition order; the local counter
        // preserves a usable order for cameras/SDKs that do not expose it.
        const auto fallbackSequence = ctx.seq_.fetch_add(1, std::memory_order_relaxed);
        f.meta.frameId              = bid != 0 ? bid : fallbackSequence;
        f.meta.timestampMs          = static_cast<std::uint64_t>(TimeHelper::NowTimestamp());
        f.width                     = static_cast<int>(width);
        f.height                    = static_cast<int>(height);
        f.channels                  = ch;

        if (depth == 8 && ch == 1)
            f.pixelFormat = PixelFormat::Mono8;
        else if (depth == 8 && ch == 3)
            f.pixelFormat = PixelFormat::BGR8;
        else
            f.pixelFormat = PixelFormat::Unknown;

        f.dataSize = nbytes;
        // NOTE: IKap 缓冲区由 SDK 管理，dispatchFrame 会在回调返回前拷贝数据。
        f.data = static_cast<const std::uint8_t *>(addr);
        return true;
    }

    // ---- 参数设置 ----

    bool setDouble(const std::string &name, double value)
    {
        return ItkDevSetDouble(ctx.cameraHandle, name.c_str(), value) == ITKSTATUS_OK;
    }

    // ---- 触发模式配置 ----

    bool set_trigger_without_grabber(const TriggerMode &trigger, std::string *errorMessage)
    {
        if (!ctx.cameraHandle)
        {
            if (errorMessage)
                *errorMessage = "device handle is null";
            return false;
        }

        enum class FrameTriggerConfigMethod
        {
            Unsupported = 0,
            Method1, // TriggerSelector + TriggerMode + ExposureMode
            Method2, // FrameTriggerEnable + FrameTriggerType
            Method3  // ExposureMode
        };

        enum class TriggerSourceConfigMethod
        {
            Unsupported = 0,
            Method1, // FrameTriggerSource
            Method2, // ExternalTriggerSource
            Method3  // TriggerSource
        };

        auto hasFeature = [this](const char *featureName) -> bool
        {
            ITK_FEATURE_BASE_INFO info{};
            return ItkDevGetFeatureInfo(ctx.cameraHandle, featureName, &info) == ITKSTATUS_OK;
        };

        auto setEnumFeature = [this, errorMessage](const char *featureName, const char *value, const char *step) -> bool
        {
            const ITKSTATUS status = ItkDevFromString(ctx.cameraHandle, featureName, value);
            if (status != ITKSTATUS_OK)
            {
                if (errorMessage)
                    *errorMessage = makeErrorMessage(std::string(step) + ": " + featureName + "=" + value, status);
                return false;
            }
            return true;
        };

        auto setBoolFeature = [this, errorMessage](const char *featureName, bool value, const char *step) -> bool
        {
            const ITKSTATUS status = ItkDevSetBool(ctx.cameraHandle, featureName, value);
            if (status != ITKSTATUS_OK)
            {
                if (errorMessage)
                    *errorMessage = makeErrorMessage(step, status);
                return false;
            }
            return true;
        };

        auto enumContains = [this](const char *featureName, const char *targetValue) -> bool
        {
            ITK_FEATURE_BASE_INFO info{};
            if (ItkDevGetFeatureInfo(ctx.cameraHandle, featureName, &info) != ITKSTATUS_OK)
                return false;
            if (info.Type != ITKFEATURE_VAL_TYPE_ENUM || info.EnumCount == 0)
                return false;
            for (std::uint32_t i = 0; i < info.EnumCount; ++i)
            {
                ITK_FEATURE_ENUM_ENTRY_INFO enumInfo{};
                if (ItkDevGetEnumEntryFeatureInfo(ctx.cameraHandle, featureName, static_cast<std::uint64_t>(i), &enumInfo) !=
                    ITKSTATUS_OK)
                    continue;
                if (std::string(enumInfo.ValueStr) == targetValue)
                    return true;
            }
            return false;
        };

        auto enumValuesToString = [this](const char *featureName) -> std::string
        {
            ITK_FEATURE_BASE_INFO info{};
            if (ItkDevGetFeatureInfo(ctx.cameraHandle, featureName, &info) != ITKSTATUS_OK)
                return "<feature not found>";
            if (info.Type != ITKFEATURE_VAL_TYPE_ENUM || info.EnumCount == 0)
                return "<not enum or empty>";
            std::ostringstream oss;
            for (std::uint32_t i = 0; i < info.EnumCount; ++i)
            {
                ITK_FEATURE_ENUM_ENTRY_INFO enumInfo{};
                if (ItkDevGetEnumEntryFeatureInfo(ctx.cameraHandle, featureName, static_cast<std::uint64_t>(i), &enumInfo) !=
                    ITKSTATUS_OK)
                    continue;
                if (i > 0)
                    oss << ", ";
                oss << enumInfo.ValueStr;
            }
            return oss.str();
        };

        auto setFirstSupportedEnumValue = [&](const char *featureName, const char *const *candidates, std::size_t candidateCount,
                                              const char *step) -> bool
        {
            for (std::size_t i = 0; i < candidateCount; ++i)
            {
                if (!enumContains(featureName, candidates[i]))
                    continue;
                if (setEnumFeature(featureName, candidates[i], step))
                    return true;
            }
            if (errorMessage)
            {
                std::ostringstream oss;
                oss << step << ": no supported enum value for " << featureName << ", available=["
                    << enumValuesToString(featureName) << "]";
                *errorMessage = oss.str();
            }
            return false;
        };

        // 探测帧触发配置方法
        FrameTriggerConfigMethod frameMethod = FrameTriggerConfigMethod::Unsupported;
        if (hasFeature("TriggerSelector") && hasFeature("TriggerMode") && hasFeature("ExposureMode"))
            frameMethod = FrameTriggerConfigMethod::Method1;
        else if (hasFeature("FrameTriggerEnable") && hasFeature("FrameTriggerType"))
            frameMethod = FrameTriggerConfigMethod::Method2;
        else if (hasFeature("ExposureMode"))
            frameMethod = FrameTriggerConfigMethod::Method3;

        // 探测触发源配置方法
        TriggerSourceConfigMethod sourceMethod = TriggerSourceConfigMethod::Unsupported;
        if (hasFeature("FrameTriggerSource"))
            sourceMethod = TriggerSourceConfigMethod::Method1;
        else if (hasFeature("ExternalTriggerSource"))
            sourceMethod = TriggerSourceConfigMethod::Method2;
        else if (hasFeature("TriggerSource"))
            sourceMethod = TriggerSourceConfigMethod::Method3;

        auto configureFrameTrigger = [&](bool enable) -> bool
        {
            switch (frameMethod)
            {
            case FrameTriggerConfigMethod::Method1:
            {
                const char *selectorCandidates[] = {"FrameStart", "FrameBurstStart"};
                if (!setFirstSupportedEnumValue("TriggerSelector", selectorCandidates,
                                                sizeof(selectorCandidates) / sizeof(selectorCandidates[0]),
                                                "set trigger selector"))
                    return false;
                return setEnumFeature("TriggerMode", enable ? "On" : "Off", "set trigger mode");
            }
            case FrameTriggerConfigMethod::Method2:
                return setBoolFeature("FrameTriggerEnable", enable, "set FrameTriggerEnable") &&
                    setEnumFeature("FrameTriggerType", "EdgeSensitive", "set FrameTriggerType");
            case FrameTriggerConfigMethod::Method3:
                return setEnumFeature("ExposureMode", enable ? "TriggerPulse" : "Timed", "set ExposureMode");
            default:
                break;
            }
            if (errorMessage)
                *errorMessage = "no supported frame trigger configuration method found";
            return false;
        };

        auto configureTriggerSource = [&](const char *sourceValue) -> bool
        {
            switch (sourceMethod)
            {
            case TriggerSourceConfigMethod::Method1:
                return setEnumFeature("FrameTriggerSource", sourceValue, "set FrameTriggerSource");
            case TriggerSourceConfigMethod::Method2:
                return setEnumFeature("ExternalTriggerSource", sourceValue, "set ExternalTriggerSource");
            case TriggerSourceConfigMethod::Method3:
                return setEnumFeature("TriggerSource", sourceValue, "set TriggerSource");
            default:
                break;
            }
            if (errorMessage)
                *errorMessage = "no supported trigger source configuration method found";
            return false;
        };

        auto configureFirstSupportedTriggerSource = [&](const char *const *candidates, std::size_t candidateCount) -> bool
        {
            const char *featureName = nullptr;
            switch (sourceMethod)
            {
            case TriggerSourceConfigMethod::Method1:
                featureName = "FrameTriggerSource";
                break;
            case TriggerSourceConfigMethod::Method2:
                featureName = "ExternalTriggerSource";
                break;
            case TriggerSourceConfigMethod::Method3:
                featureName = "TriggerSource";
                break;
            default:
                if (errorMessage)
                    *errorMessage = "no supported trigger source configuration method found";
                return false;
            }
            return setFirstSupportedEnumValue(featureName, candidates, candidateCount, "set trigger source");
        };

        switch (trigger)
        {
        case TriggerMode::Continuous:
            return configureFrameTrigger(false);
        case TriggerMode::Software:
            return configureFrameTrigger(true) && configureTriggerSource("Software");
        case TriggerMode::Hardware:
        {
            const char *sourceCandidates[] = {"Line1", "Line0", "Line2", "Counter0", "Action1"};
            return configureFrameTrigger(true) &&
                configureFirstSupportedTriggerSource(sourceCandidates, sizeof(sourceCandidates) / sizeof(sourceCandidates[0]));
        }
        default:
            if (errorMessage)
                *errorMessage = "unsupported trigger mode";
            return false;
        }

        clearError(errorMessage);
        return true;
    }

    bool software_trigger_grab(std::string *errorMessage)
    {
        if (!ctx.cameraHandle)
        {
            if (errorMessage)
                *errorMessage = "device handle is null";
            return false;
        }
        const auto res = ItkDevExecuteCommand(ctx.cameraHandle, "TriggerSoftware");
        if (res != ITKSTATUS_OK)
        {
            if (errorMessage)
                *errorMessage = makeErrorMessage("TriggerSoftware failed", res);
            return false;
        }
        clearError(errorMessage);
        return true;
    }
};

// ========================================================================
// CameraIkapSdkAdapter (PIMPL facade)
// ========================================================================

CameraIkapSdkAdapter::CameraIkapSdkAdapter(CameraInfo info) :
    info_(std::move(info)),
    impl_(std::make_unique<Impl>(this, info_))
{
}

CameraIkapSdkAdapter::~CameraIkapSdkAdapter() = default;

std::vector<CameraInfo> CameraIkapSdkAdapter::enumerateDevices(std::string *errorMessage)
{
    clearError(errorMessage);
    return {info_};
}

bool CameraIkapSdkAdapter::open(std::string *errorMessage)
{
    const auto state = impl_->state.load(std::memory_order_relaxed);
    if (state == CameraState::Opened)
    {
        clearError(errorMessage);
        return true;
    }
    const bool ok = impl_->open(errorMessage);
    if (ok)
        LOGM_INFO(LOG_TAG, "CameraIkapSdkAdapter open success, logical_name={}", info_.logicalName);
    else
        LOGM_ERROR(LOG_TAG, "CameraIkapSdkAdapter open failed, logical_name={}", info_.logicalName);
    return ok;
}

bool CameraIkapSdkAdapter::close(std::string *errorMessage)
{
    clearError(errorMessage);
    impl_->cleanup();
    LOGM_INFO(LOG_TAG, "CameraIkapSdkAdapter close, logical_name={}", info_.logicalName);
    return true;
}

bool CameraIkapSdkAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage)
{
    clearError(errorMessage);
    const auto state       = impl_->state.load(std::memory_order_relaxed);
    const bool isIdleLike  = state == CameraState::Opened || state == CameraState::Configured || state == CameraState::Stopped;
    const bool isStreaming = state == CameraState::Streaming;

    if (!isIdleLike && !isStreaming)
    {
        if (errorMessage)
            *errorMessage = "camera state does not allow applyConfig";
        LOGM_ERROR(LOG_TAG, "CameraIkapSdkAdapter applyConfig rejected, logical_name={}, state={}", info_.logicalName,
                   static_cast<int>(state));
        return false;
    }

    if (isStreaming)
    {
        const bool wantsTriggerModeChange = config.triggerMode != impl_->currentConfig.triggerMode;
        const int newBufferCount          = config.bufferCount > 0 ? config.bufferCount : 72;
        const bool wantsBufferCountChange = newBufferCount != impl_->bufferCount;

        if (wantsTriggerModeChange || wantsBufferCountChange)
        {
            if (errorMessage)
                *errorMessage = "triggerMode/bufferCount cannot be changed while streaming";
            LOGM_ERROR(LOG_TAG,
                       "CameraIkapSdkAdapter applyConfig rejected while streaming, logical_name={}, "
                       "triggerModeChange={}, bufferCountChange={}",
                       info_.logicalName, wantsTriggerModeChange, wantsBufferCountChange);
            return false;
        }

        if (config.exposureTime > 0.0 && !impl_->setDouble("ExposureTime", config.exposureTime))
        {
            if (errorMessage)
                *errorMessage = "set ExposureTime failed";
            return false;
        }
        if (config.gain > 0.0 && !impl_->setDouble("DigitalGain", config.gain))
        {
            if (errorMessage)
                *errorMessage = "set DigitalGain failed";
            return false;
        }

        if (config.exposureTime > 0.0)
            impl_->currentConfig.exposureTime = config.exposureTime;
        if (config.gain > 0.0)
            impl_->currentConfig.gain = config.gain;

        clearError(errorMessage);
        LOGM_INFO(LOG_TAG, "CameraIkapSdkAdapter applyConfig success while streaming, logical_name={}, exposure={}, gain={}",
                  info_.logicalName, config.exposureTime, config.gain);
        return true;
    }

    // 非 streaming：全量应用
    impl_->currentConfig = config;
    impl_->bufferCount   = config.bufferCount > 0 ? config.bufferCount : 72;

    if (!impl_->set_trigger_without_grabber(config.triggerMode, errorMessage))
    {
        LOGM_ERROR(LOG_TAG, "CameraIkapSdkAdapter set trigger mode failed, logical_name={}, value={}", info_.logicalName,
                   static_cast<int>(config.triggerMode));
        return false;
    }

    if (config.exposureTime > 0.0 && !impl_->setDouble("ExposureTime", config.exposureTime))
    {
        if (errorMessage)
            *errorMessage = "set ExposureTime failed";
        return false;
    }
    if (config.gain > 0.0 && !impl_->setDouble("DigitalGain", config.gain))
    {
        if (errorMessage)
            *errorMessage = "set DigitalGain failed";
        return false;
    }

    clearError(errorMessage);
    return true;
}

bool CameraIkapSdkAdapter::startGrabbing(std::string *errorMessage)
{
    return impl_->start_stream(errorMessage);
}

bool CameraIkapSdkAdapter::stopGrabbing(std::string *errorMessage)
{
    return impl_->stop_stream(errorMessage);
}

bool CameraIkapSdkAdapter::executeSoftwareTrigger(std::string *errorMessage)
{
    return impl_->software_trigger_grab(errorMessage);
}

void CameraIkapSdkAdapter::setFrameCallback(FrameCallback callback)
{
    impl_->frameCallback = std::move(callback);
}

CameraInfo CameraIkapSdkAdapter::info() const
{
    return info_;
}

// ========================================================================
// IkapCameraSdkFactory
// ========================================================================

std::vector<CameraInfo> IkapCameraSdkFactory::enumerateDevices(std::string *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    // 清空上次缓存，准备重新填充
    deviceIds_.clear();
    serialNumbers_.clear();
    logicalNames_.clear();
    vendors_.clear();
    models_.clear();

    std::string runtimeError;
    if (!IkapSDKRuntime::instance().acquire(&runtimeError))
    {
        if (errorMessage)
            *errorMessage = runtimeError;
        LOGM_ERROR(LOG_TAG, "IKap enumerateDevices failed to acquire runtime: {}", runtimeError);
        return {};
    }

    uint32_t count   = 0;
    ITKSTATUS status = ItkManGetDeviceCount(&count);
    if (status != ITKSTATUS_OK)
    {
        if (errorMessage)
            *errorMessage = "ItkManGetDeviceCount failed, status=" + std::to_string(status);
        return {};
    }

    auto safeStr = [](const char *s) -> std::string
    {
        return s ? std::string(s) : std::string();
    };

    deviceIds_.reserve(count);
    serialNumbers_.reserve(count);
    logicalNames_.reserve(count);
    vendors_.reserve(count);
    models_.reserve(count);

    std::vector<CameraInfo> devices;
    devices.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        ITKDEV_INFO info{};
        status = ItkManGetDeviceInfo(i, &info);
        if (status != ITKSTATUS_OK)
            continue;
        if (std::string(info.SerialNumber).empty())
            continue;

        // 字符串存入私有缓存，CameraInfo 指针指向缓存内数据
        deviceIds_.push_back(safeStr(info.FullName));
        serialNumbers_.push_back(safeStr(info.SerialNumber));
        logicalNames_.push_back("camera_" + std::to_string(i));
        vendors_.push_back(safeStr(info.VendorName));
        models_.push_back(safeStr(info.ModelName));

        CameraInfo cameraInfo;
        cameraInfo.deviceId     = deviceIds_.back().c_str();
        cameraInfo.serialNumber = serialNumbers_.back().c_str();
        cameraInfo.logicalName  = logicalNames_.back().c_str();
        cameraInfo.vendor       = vendors_.back().c_str();
        cameraInfo.model        = models_.back().c_str();
        cameraInfo.role         = CameraRole::Unknown;

        devices.push_back(cameraInfo);
    }

    return devices;
}

std::unique_ptr<ICameraSdkAdapter> IkapCameraSdkFactory::createAdapter(const CameraInfo &info, std::string *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    try
    {
        return std::make_unique<CameraIkapSdkAdapter>(info);
    } catch (const std::exception &e)
    {
        if (errorMessage)
            *errorMessage = e.what();
        return nullptr;
    }
}

} // namespace SZXCamera
