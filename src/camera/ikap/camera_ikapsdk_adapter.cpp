#include "camera/ikap/camera_ikapsdk_adapter.h"
#include <sstream>

namespace SZXCamera {
    
    namespace{

        void clearError(std::string *errorMessage){
                if (errorMessage) {
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
            if (!startsWithCameraPrefix(logicalName)) {
                return -1;
            }

            try {
                return std::stoi(logicalName.substr(std::string("camera_").size()));
            } catch (...) {
                return -1;
            }
        }

        bool matchesCameraInfo(const CameraInfo &info, const ITKDEV_INFO &deviceInfo, std::uint32_t index)
        {
            if (!info.serialNumber.empty()) {
                if (info.serialNumber == std::string(deviceInfo.SerialNumber)) {
                    return true;
                }
            }

            if (!info.deviceId.empty()) {
                if (info.deviceId == std::string(deviceInfo.FullName)) {
                    return true;
                }
            }

            const int logicalIndex = tryParseLogicalCameraIndex(info.logicalName);
            if (logicalIndex >= 0 && static_cast<std::uint32_t>(logicalIndex) == index) {
                return true;
            }

            return false;
        }

        inline int logicalIndexFromName(const std::string &name){
            constexpr const char *prefix = "camera_";
            if (name.rfind(prefix, 0) != 0) {
                return -1;
            }

            try {
                return std::stoi(name.substr(std::strlen(prefix)));
            } catch (...) {
                return -1;
            }
        }
    }

    std::vector<CameraInfo> IkapCameraSdkFactory::enumerateDevices(std::string *errorMessage)
    {
        if (errorMessage) {
            errorMessage->clear();
        }

        std::vector<CameraInfo> devices;

        std::string runtimeError;
        if (!IkapSDKRuntime::instance().acquire(&runtimeError)) {
            if (errorMessage) {
                *errorMessage = runtimeError;
            }
            LOG_ERROR("IKap enumerateDevices failed to acquire runtime: {}", runtimeError);
            return devices;
        }

        uint32_t count = 0;
        ITKSTATUS status = ItkManGetDeviceCount(&count);
        if (status != ITKSTATUS_OK) {
            if (errorMessage) {
                *errorMessage = "ItkManGetDeviceCount failed, status=" + std::to_string(status);
            }
            return devices;
        }

        auto safeStr = [](const char* s) -> std::string {
            return s ? std::string(s) : std::string();
        };

        for (uint32_t i = 0; i < count; ++i) {
            ITKDEV_INFO info{};
            status = ItkManGetDeviceInfo(i, &info);
            if (status != ITKSTATUS_OK) {
                continue;
            }

            // 序列号为空的设备直接跳过
            if (std::string(info.SerialNumber).empty()) {
                continue;
            }

            CameraInfo cameraInfo;
            cameraInfo.deviceId = safeStr(info.FullName);                 // IKap 中用于唯一识别设备的名字
            cameraInfo.serialNumber = safeStr(info.SerialNumber);         // 序列号
            cameraInfo.logicalName = "camera_" + std::to_string(i); // 业务逻辑名称，暂时用 camera_0、camera_1... 可以改成更友好的名字
            cameraInfo.vendor = safeStr(info.VendorName);                // 厂商
            cameraInfo.model = safeStr(info.ModelName);                  // 型号
            cameraInfo.role = CameraRole::Unknown;             // 先默认未知，后续可以根据 ModelName 或其他信息判断角色
            devices.push_back(std::move(cameraInfo));
        }

        return devices;
    }

    std::unique_ptr<ICameraSdkAdapter> IkapCameraSdkFactory::createAdapter(const CameraInfo &info,
                                        std::string *errorMessage)
    {
        if (errorMessage) errorMessage->clear();

        try {
            return std::make_unique<CameraIkapSdkAdapter>(info);
        } catch (const std::exception& e) {
            if (errorMessage) {
                *errorMessage = e.what();
            }
            return nullptr;
        }
    }

    std::vector<std::unique_ptr<ICameraSdkAdapter>> IkapCameraSdkFactory::createAllAdapters(){
        std::vector<std::unique_ptr<ICameraSdkAdapter>> adapters;

        std::string err;
        auto devices = enumerateDevices(&err);

        if (!err.empty()) {
            LOG_ERROR("enumerateDevices failed: " + err);
            return adapters;
        }

        if (devices.empty()) {
            LOG_WARN("No IKap devices found");
            return adapters;
        }

        adapters.reserve(devices.size());

        for (const auto& dev : devices) {
            std::string createErr;

            auto adapter = createAdapter(dev, &createErr);

            if (!adapter) {
                LOG_ERROR("createAdapter failed for device: " + dev.deviceId +
                        " err=" + createErr);
                continue;
            }

            adapters.emplace_back(std::move(adapter));
        }

        return adapters;
    }

    struct  CameraIkapSdkAdapter::Impl{

        struct DeviceContext{
            std::uint32_t deviceIndex{std::numeric_limits<std::uint32_t>::max()};

            ITKDEVICE cameraHandle{nullptr};
            ITKSTREAM streamHandle{nullptr};
            ITKBUFFER convertBuffer{nullptr};
            ITKDEV_INFO deviceInfo{}; ///< 设备信息结构体。

            std::uint32_t grabCount{ITKSTREAM_CONTINUOUS};
            std::atomic<std::uint64_t> seq_{1};

            void resetHandles()
            {
                cameraHandle = nullptr;
                streamHandle = nullptr;
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
                deviceInfo = {};
            }

            void resetAll()
            {
                resetHandles();
                resetDeviceBinding();
                grabCount = ITKSTREAM_CONTINUOUS;
            }

            /**
             * @brief 查找 IKap 设备（支持多种匹配方式）
             *
             * 查找优先级：
             *   1. serialNumber（最可靠）
             *   2. deviceId（FullName）
             *   3. logicalName → index
             *
             * @param[in]  info   用户提供的相机标识信息
             * @param[out] idx    找到的设备索引（用于 ItkDevOpen）
             * @param[out] out    对应设备信息（ITKDEV_INFO）
             *
             * @return true  找到设备
             * @return false 未找到设备 / 无设备 / SDK异常
             */
            static bool find_device(const CameraInfo &info, std::uint32_t &idx, ITKDEV_INFO &out){
                std::uint32_t cnt = 0;
                if (ItkManGetDeviceCount(&cnt) != ITKSTATUS_OK || cnt == 0) {
                    return false;
                }

                if (!info.serialNumber.empty()) {
                    for (std::uint32_t i = 0; i < cnt; ++i) {
                        ITKDEV_INFO t{};
                        if (ItkManGetDeviceInfo(i, &t) != ITKSTATUS_OK) {
                            continue;
                        }
                        if (info.serialNumber == std::string(t.SerialNumber)) {
                            idx = i;
                            out = t;
                            return true;
                        }
                    }
                }

                if (!info.deviceId.empty()) {
                    for (std::uint32_t i = 0; i < cnt; ++i) {
                        ITKDEV_INFO t{};
                        if (ItkManGetDeviceInfo(i, &t) != ITKSTATUS_OK) {
                            continue;
                        }
                        if (info.deviceId == std::string(t.FullName)) {
                            idx = i;
                            out = t;
                            return true;
                        }
                    }
                }

                const int logicalIndex = logicalIndexFromName(info.logicalName);
                if (logicalIndex >= 0 && static_cast<std::uint32_t>(logicalIndex) < cnt) {
                    ITKDEV_INFO t{};
                    if (ItkManGetDeviceInfo(static_cast<std::uint32_t>(logicalIndex), &t) == ITKSTATUS_OK) {
                        idx = static_cast<std::uint32_t>(logicalIndex);
                        out = t;
                        return true;
                    }
                }

                return false;
            }
        };
        
        bool runtime_acquired{false};
        int bufferCount{72};
        std::atomic_bool cb_installed{false};
        FrameCallback frameCallback{nullptr};
        std::atomic<CameraState> state{CameraState::Created};
        CameraIkapSdkAdapter *owner{nullptr};
        DeviceContext ctx{};
        CameraConfig currentConfig{};
        CameraInfo boundInfo{};
        
        explicit Impl(CameraIkapSdkAdapter *self, CameraInfo info): boundInfo(std::move(info)),owner(self)
            {
                state.store(CameraState::Created, std::memory_order_relaxed);
            }

        ~Impl()
        {
            cleanup();
            release_runtime();
        }


        // 设置触发模式。
        // 设计目标：
        // 1. 参考 IKap 官方 ConfigureCamera.cpp 的探测思路，先判断相机支持哪套配置路径。
        // 2. 对 METHOD_1（TriggerSelector + TriggerMode + ExposureMode）进一步细化：
        //    不能写死 TriggerSelector=FrameStart，而是要根据真实枚举值选择。
        // 3. 当前 TriggerMode 枚举只表达：连续 / 软件触发 / 硬触发。
        //    因此这里统一按“边沿触发”思路处理，不做 level-sensitive 的分支。
        bool set_trigger_without_grabber(const TriggerMode &trigger, std::string *errorMessage)
        {
            if (!ctx.cameraHandle) {
                if (errorMessage) {
                    *errorMessage = "device handle is null";
                }
                return false;
            }

            enum class FrameTriggerConfigMethod {
                Unsupported = 0,
                Method1, // TriggerSelector + TriggerMode + ExposureMode
                Method2, // FrameTriggerEnable + FrameTriggerType
                Method3  // ExposureMode
            };

            enum class TriggerSourceConfigMethod {
                Unsupported = 0,
                Method1, // FrameTriggerSource
                Method2, // ExternalTriggerSource
                Method3  // TriggerSource
            };

            auto hasFeature = [this](const char *featureName) -> bool {
                ITK_FEATURE_BASE_INFO info{};
                return ItkDevGetFeatureInfo(ctx.cameraHandle, featureName, &info) == ITKSTATUS_OK;
            };

            auto setEnumFeature = [this, errorMessage](const char *featureName,
                                                       const char *value,
                                                       const char *step) -> bool {
                const ITKSTATUS status = ItkDevFromString(ctx.cameraHandle, featureName, value);
                if (status != ITKSTATUS_OK) {
                    if (errorMessage) {
                        *errorMessage = makeErrorMessage(
                            std::string(step) + ": " + featureName + "=" + value, status);
                    }
                    return false;
                }
                return true;
            };

            auto setBoolFeature = [this, errorMessage](const char *featureName,
                                                       bool value,
                                                       const char *step) -> bool {
                const ITKSTATUS status = ItkDevSetBool(ctx.cameraHandle, featureName, value);
                if (status != ITKSTATUS_OK) {
                    if (errorMessage) {
                        *errorMessage = makeErrorMessage(step, status);
                    }
                    return false;
                }
                return true;
            };

            auto enumContains = [this](const char *featureName, const char *targetValue) -> bool {
                ITK_FEATURE_BASE_INFO info{};
                if (ItkDevGetFeatureInfo(ctx.cameraHandle, featureName, &info) != ITKSTATUS_OK) {
                    return false;
                }

                if (info.Type != ITKFEATURE_VAL_TYPE_ENUM || info.EnumCount == 0) {
                    return false;
                }

                for (std::uint32_t i = 0; i < info.EnumCount; ++i) {
                    ITK_FEATURE_ENUM_ENTRY_INFO enumInfo{};
                    if (ItkDevGetEnumEntryFeatureInfo(
                            ctx.cameraHandle,
                            featureName,
                            static_cast<std::uint64_t>(i),
                            &enumInfo) != ITKSTATUS_OK) {
                        continue;
                    }

                    if (std::string(enumInfo.ValueStr) == targetValue) {
                        return true;
                    }
                }

                return false;
            };

            auto enumValuesToString = [this](const char *featureName) -> std::string {
                ITK_FEATURE_BASE_INFO info{};
                if (ItkDevGetFeatureInfo(ctx.cameraHandle, featureName, &info) != ITKSTATUS_OK) {
                    return "<feature not found>";
                }

                if (info.Type != ITKFEATURE_VAL_TYPE_ENUM || info.EnumCount == 0) {
                    return "<not enum or empty>";
                }

                std::ostringstream oss;
                for (std::uint32_t i = 0; i < info.EnumCount; ++i) {
                    ITK_FEATURE_ENUM_ENTRY_INFO enumInfo{};
                    if (ItkDevGetEnumEntryFeatureInfo(
                            ctx.cameraHandle,
                            featureName,
                            static_cast<std::uint64_t>(i),
                            &enumInfo) != ITKSTATUS_OK) {
                        continue;
                    }

                    if (i > 0) {
                        oss << ", ";
                    }
                    oss << enumInfo.ValueStr;
                }
                return oss.str();
            };

            auto setFirstSupportedEnumValue =
                [&](const char *featureName,
                    const char *const *candidates,
                    std::size_t candidateCount,
                    const char *step) -> bool {
                    for (std::size_t i = 0; i < candidateCount; ++i) {
                        const char *candidate = candidates[i];
                        if (!enumContains(featureName, candidate)) {
                            continue;
                        }

                        if (setEnumFeature(featureName, candidate, step)) {
                            return true;
                        }
                    }

                    if (errorMessage) {
                        std::ostringstream oss;
                        oss << step
                            << ": no supported enum value for " << featureName
                            << ", available=[" << enumValuesToString(featureName) << "]";
                        *errorMessage = oss.str();
                    }
                    return false;
                };

            // 按官方示例的思路探测“帧触发配置方法”。
            FrameTriggerConfigMethod frameMethod = FrameTriggerConfigMethod::Unsupported;
            if (hasFeature("TriggerSelector") &&
                hasFeature("TriggerMode") &&
                hasFeature("ExposureMode")) {
                frameMethod = FrameTriggerConfigMethod::Method1;
            } else if (hasFeature("FrameTriggerEnable") &&
                       hasFeature("FrameTriggerType")) {
                frameMethod = FrameTriggerConfigMethod::Method2;
            } else if (hasFeature("ExposureMode")) {
                frameMethod = FrameTriggerConfigMethod::Method3;
            }

            // 按官方示例的思路探测“触发源配置方法”。
            TriggerSourceConfigMethod sourceMethod = TriggerSourceConfigMethod::Unsupported;
            if (hasFeature("FrameTriggerSource")) {
                sourceMethod = TriggerSourceConfigMethod::Method1;
            } else if (hasFeature("ExternalTriggerSource")) {
                sourceMethod = TriggerSourceConfigMethod::Method2;
            } else if (hasFeature("TriggerSource")) {
                sourceMethod = TriggerSourceConfigMethod::Method3;
            }

            // 配置“帧触发开/关”。
            // 对 METHOD_1，这里不再写死 TriggerSelector=FrameStart。
            // 优先尝试 FrameStart；如果相机只支持 FrameBurstStart，则自动退到后者。
            auto configureFrameTrigger = [&](bool enable) -> bool {
                switch (frameMethod) {
                case FrameTriggerConfigMethod::Method1: {
                    const char *selectorCandidates[] = {
                        "FrameStart",
                        "FrameBurstStart"
                    };

                    if (!setFirstSupportedEnumValue(
                            "TriggerSelector",
                            selectorCandidates,
                            sizeof(selectorCandidates) / sizeof(selectorCandidates[0]),
                            "set trigger selector")) {
                        return false;
                    }

                    if (!setEnumFeature(
                            "TriggerMode",
                            enable ? "On" : "Off",
                            "set trigger mode")) {
                        return false;
                    }
                    return true;
                }

                case FrameTriggerConfigMethod::Method2:
                    if (!setBoolFeature(
                            "FrameTriggerEnable",
                            enable,
                            "set FrameTriggerEnable")) {
                        return false;
                    }

                    // 当前仅处理边沿触发。
                    if (!setEnumFeature(
                            "FrameTriggerType",
                            "EdgeSensitive",
                            "set FrameTriggerType")) {
                        return false;
                    }
                    return true;

                case FrameTriggerConfigMethod::Method3:
                    // 官方示例中：
                    // - 开启帧触发 -> TriggerPulse
                    // - 关闭帧触发 -> Timed
                    if (!setEnumFeature(
                            "ExposureMode",
                            enable ? "TriggerPulse" : "Timed",
                            "set ExposureMode")) {
                        return false;
                    }
                    return true;

                case FrameTriggerConfigMethod::Unsupported:
                default:
                    break;
                }

                if (errorMessage) {
                    *errorMessage = "no supported frame trigger configuration method found";
                }
                return false;
            };

            auto configureTriggerSource = [&](const char *sourceValue) -> bool {
                switch (sourceMethod) {
                case TriggerSourceConfigMethod::Method1:
                    return setEnumFeature("FrameTriggerSource", sourceValue, "set FrameTriggerSource");

                case TriggerSourceConfigMethod::Method2:
                    return setEnumFeature("ExternalTriggerSource", sourceValue, "set ExternalTriggerSource");

                case TriggerSourceConfigMethod::Method3:
                    return setEnumFeature("TriggerSource", sourceValue, "set TriggerSource");

                case TriggerSourceConfigMethod::Unsupported:
                default:
                    break;
                }

                if (errorMessage) {
                    *errorMessage = "no supported trigger source configuration method found";
                }
                return false;
            };

            auto configureFirstSupportedTriggerSource =
                [&](const char *const *candidates, std::size_t candidateCount) -> bool {
                    const char *featureName = nullptr;
                    switch (sourceMethod) {
                    case TriggerSourceConfigMethod::Method1:
                        featureName = "FrameTriggerSource";
                        break;
                    case TriggerSourceConfigMethod::Method2:
                        featureName = "ExternalTriggerSource";
                        break;
                    case TriggerSourceConfigMethod::Method3:
                        featureName = "TriggerSource";
                        break;
                    case TriggerSourceConfigMethod::Unsupported:
                    default:
                        if (errorMessage) {
                            *errorMessage = "no supported trigger source configuration method found";
                        }
                        return false;
                    }

                    return setFirstSupportedEnumValue(
                        featureName,
                        candidates,
                        candidateCount,
                        "set trigger source");
                };

            switch (trigger) {
            case TriggerMode::Continuous:
                // 连续采集 = 关闭帧触发
                if (!configureFrameTrigger(false)) {
                    return false;
                }
                break;

            case TriggerMode::Software:
                // 软件触发 = 开启帧触发 + 触发源=Software
                // 真正发一次软触发命令，不在这里做，
                // 仍然由 software_trigger_grab() 调 TriggerSoftware 完成。
                if (!configureFrameTrigger(true)) {
                    return false;
                }

                if (!configureTriggerSource("Software")) {
                    return false;
                }
                break;

            case TriggerMode::Hardware: {
                // 硬触发 = 开启帧触发 + 触发源=某个外部输入
                if (!configureFrameTrigger(true)) {
                    return false;
                }

                const char *sourceCandidates[] = {
                    "Line1",
                    "Line0",
                    "Line2",
                    "Counter0",
                    "Action1"
                };

                if (!configureFirstSupportedTriggerSource(
                        sourceCandidates,
                        sizeof(sourceCandidates) / sizeof(sourceCandidates[0]))) {
                    return false;
                }
                break;
            }

            default:
                if (errorMessage) {
                    *errorMessage = "unsupported trigger mode";
                }
                return false;
            }

            clearError(errorMessage);
            return true;
        }



        bool software_trigger_grab(std::string *errorMessage)
        {
            if (!ctx.cameraHandle) {
                if (errorMessage) {
                    *errorMessage = "device handle is null";
                }
                return false;
            }

            const auto res = ItkDevExecuteCommand(ctx.cameraHandle, "TriggerSoftware");
            if (res != ITKSTATUS_OK) {
                if (errorMessage) {
                    *errorMessage = makeErrorMessage("TriggerSoftware failed", res);
                }
                return false;
            }

            clearError(errorMessage);
            return true;
        }

        bool open(std::string* errorMessage){
            clearError(errorMessage);
            if (!ensure_runtime(errorMessage)) {
                return false;
            }

            if (!ctx.find_device(boundInfo, ctx.deviceIndex, ctx.deviceInfo)) {
                if (errorMessage) {
                    *errorMessage = "find_device failed";
                }
                return false;
            }

            const auto status = ItkDevOpen(ctx.deviceIndex, ITKDEV_VAL_ACCESS_MODE_EXCLUSIVE, &ctx.cameraHandle);
            if (status != ITKSTATUS_OK || !ctx.cameraHandle) {
                if (errorMessage) {
                    *errorMessage = makeErrorMessage("ItkDevOpen failed", status);
                }
                return false;
            }

            state.store(CameraState::Opened, std::memory_order_relaxed);
            clearError(errorMessage);
            return true;
        }

        bool start_stream(std::string *errorMessage){
            clearError(errorMessage);

            if (!ctx.cameraHandle) {
                if (errorMessage) {
                    *errorMessage = "device is not opened";
                }
                return false;
            }

            if (!create_stream_and_cvtbuf(errorMessage)) {
                return false;
            }

            if (!config_stream(errorMessage)) {
                return false;
            }

            const auto reg = ItkStreamRegisterCallback(
                ctx.streamHandle,
                ITKSTREAM_VAL_EVENT_TYPE_END_OF_FRAME,
                on_eof,
                this);

            if (reg != ITKSTATUS_OK) {
                if (errorMessage) {
                    *errorMessage = makeErrorMessage("ItkStreamRegisterCallback failed", reg);
                }
                return false;
            }

            cb_installed = true;

            const auto st = ItkStreamStart(ctx.streamHandle, ITKSTREAM_CONTINUOUS);
            if (st != ITKSTATUS_OK) {
                if (errorMessage) {
                    *errorMessage = makeErrorMessage("ItkStreamStart failed", st);
                }
                return false;
            }

            state.store(CameraState::Streaming, std::memory_order_relaxed);
            clearError(errorMessage);
            return true;
        }

        bool create_stream_and_cvtbuf(std::string *errorMessage)
        {
            if (!ctx.cameraHandle) {
                if (errorMessage) {
                    *errorMessage = "device handle is null";
                }
                return false;
            }

            std::uint32_t streamCount = 0;
            if (ItkDevGetStreamCount(ctx.cameraHandle, &streamCount) != ITKSTATUS_OK || streamCount == 0) {
                if (errorMessage) {
                    *errorMessage = "device has no stream channel";
                }
                return false;
            }

            if (ItkDevAllocStreamEx(ctx.cameraHandle, 0, bufferCount, &ctx.streamHandle) != ITKSTATUS_OK || !ctx.streamHandle) {
                if (errorMessage) {
                    *errorMessage = "ItkDevAllocStreamEx failed";
                }
                return false;
            }

            ITKBUFFER hBuf0 = nullptr;
            if (ItkStreamGetBuffer(ctx.streamHandle, 0, &hBuf0) != ITKSTATUS_OK || !hBuf0) {
                if (errorMessage) {
                    *errorMessage = "ItkStreamGetBuffer(0) failed";
                }
                return false;
            }

            ITK_BUFFER_INFO bi = {0};
            if (ItkBufferGetInfo(hBuf0, &bi) != ITKSTATUS_OK) {
                if (errorMessage) {
                    *errorMessage = "ItkBufferGetInfo failed";
                }
                return false;
            }

            if (!ctx.convertBuffer) {
                if (ItkBufferNew(bi.ImageWidth, bi.ImageHeight,
                                ITKBUFFER_VAL_FORMAT_BGR161616,
                                &ctx.convertBuffer) != ITKSTATUS_OK || !ctx.convertBuffer) {
                    if (errorMessage) {
                        *errorMessage = "ItkBufferNew(ctx.convertBuffer) failed";
                    }
                    return false;
                }
            }

            clearError(errorMessage);
            return true;
        }

        bool config_stream(std::string *errorMessage)
        {
            if (!ctx.streamHandle) {
                if (errorMessage) {
                    *errorMessage = "stream handle is null";
                }
                return false;
            }

            const std::uint32_t startMode = ITKSTREAM_VAL_START_MODE_NON_BLOCK;
            const std::uint32_t xferMode  = ITKSTREAM_VAL_TRANSFER_MODE_SYNCHRONOUS_WITH_PROTECT;
            const std::uint32_t timeout   = INFINITE;
            const std::uint32_t autoClear = 0;

            const ITKSTATUS s1 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_START_MODE,    &startMode);
            const ITKSTATUS s2 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_TRANSFER_MODE, &xferMode);
            const ITKSTATUS s3 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_TIME_OUT,      &timeout);
            const ITKSTATUS s4 = ItkStreamSetPrm(ctx.streamHandle, ITKSTREAM_PRM_AUTO_CLEAR,    &autoClear);

            if (!(s1 == ITKSTATUS_OK && s2 == ITKSTATUS_OK && s3 == ITKSTATUS_OK && s4 == ITKSTATUS_OK)) {
                if (errorMessage) {
                    *errorMessage = "config_stream failed";
                }
                return false;
            }

            clearError(errorMessage);
            return true;
        }

        static void IKAPC_CC on_eof(std::uint32_t /*eventType*/, void *ctx)
        {
            auto *self = static_cast<Impl *>(ctx);
            if (!self || !self->frameCallback || !self->ctx.streamHandle) {
                return;
            }

            ITKBUFFER buf = nullptr;
            std::uint32_t bStatus = 0;

            ItkStreamGetPrm(self->ctx.streamHandle, ITKSTREAM_PRM_CURRENT_BUFFER_HANDLE, &buf);
            if (!buf) {
                return;
            }

            ItkBufferGetPrm(buf, ITKBUFFER_PRM_STATE, &bStatus);
            if (bStatus == ITKBUFFER_VAL_STATE_FULL) {
                self->dispatch(buf);
            }

            ItkStreamClearBuffer(self->ctx.streamHandle, buf);
        }

        void dispatch(ITKBUFFER buf)
        {
            FramePacket frame;
            if (!make_frame(buf, frame)) {
                return;
            }
            std::shared_ptr<const FramePacket> sharedFrame = std::make_shared<FramePacket>(std::move(frame));
            frameCallback(sharedFrame);
        }

        bool make_frame(ITKBUFFER buf, FramePacket &f)
        {
            if (!buf) {
                return false;
            }

            std::uint32_t stateVal = 0;
            ItkBufferGetPrm(buf, ITKBUFFER_PRM_STATE, &stateVal);
            if (stateVal != ITKBUFFER_VAL_STATE_FULL) {
                return false;
            }

            std::uint8_t need = 0;
            ItkBufferNeedAutoConvert(buf, &need);

            ITKBUFFER rd = buf;
            if (need) {
                if (!ctx.convertBuffer) {
                    return false;
                }
                if (ItkBufferConvert(buf, ctx.convertBuffer, 0, ITKBUFFER_VAL_CONVERT_OPTION_AUTO_FORMAT) != ITKSTATUS_OK) {
                    return false;
                }
                rd = ctx.convertBuffer;
            }

            int depth = 0;
            int bits = 0;
            std::int64_t w = 0;
            std::int64_t h = 0;
            void *addr = nullptr;
            std::uint64_t bid = 0;

            ItkBufferGetPrm(rd, ITKBUFFER_PRM_PIXEL_DEPTH, &depth);
            ItkBufferGetPrm(rd, ITKBUFFER_PRM_DATA_BIT,    &bits);
            ItkBufferGetPrm(rd, ITKBUFFER_PRM_WIDTH,       &w);
            ItkBufferGetPrm(rd, ITKBUFFER_PRM_HEIGHT,      &h);
            ItkBufferGetPrm(rd, ITKBUFFER_PRM_ADDRESS,     &addr);
            ItkBufferGetPrm(rd, ITKBUFFER_PRM_BLOCK_ID,    &bid);

            if (!addr || w <= 0 || h <= 0) {
                return false;
            }

            const int bytesPerCh = (depth == 8 ? 1 : 2);
            const int denom      = (depth == 8 ? 8 : 16);
            const int ch         = denom ? (bits / denom) : 0;

            const std::uint32_t width  = static_cast<std::uint32_t>(w);
            const std::uint32_t height = static_cast<std::uint32_t>(h);
            const std::size_t stride   = width * static_cast<std::uint32_t>(ch) * static_cast<std::uint32_t>(bytesPerCh);
            const std::size_t nbytes   = stride * height;

              f = {};
              f.meta.cameraName = boundInfo.logicalName;
              f.meta.cameraSerialNumber = boundInfo.serialNumber;
              f.meta.frameId = ctx.seq_.fetch_add(1, std::memory_order_relaxed);
              f.meta.timestamp = std::chrono::system_clock::now();

            f.width = static_cast<int>(width);
            f.height = static_cast<int>(height);
            f.channels = ch;

            if (depth == 8 && ch == 1) {
                f.pixelFormat = PixelFormat::Mono8;
            } else if (depth == 8 && ch == 3) {
                f.pixelFormat = PixelFormat::BGR8;
            } else {
                f.pixelFormat = PixelFormat::Unknown;
            }

            f.data.resize(nbytes);
            std::memcpy(f.data.data(), addr, nbytes);
            return true;
        }

        void release_stream_resources()
        {
            if (ctx.streamHandle) {
                ItkStreamStop(ctx.streamHandle);

                if (cb_installed) {
                    ItkStreamUnregisterCallback(ctx.streamHandle, ITKSTREAM_VAL_EVENT_TYPE_END_OF_FRAME);
                    cb_installed = false;
                }

                ItkDevFreeStream(ctx.streamHandle);
                ctx.resetStreamHandle();
            }

            if (ctx.convertBuffer) {
                ItkBufferFree(ctx.convertBuffer);
                ctx.resetConvertBuffer();
            }
        }

        bool stop_stream(std::string *errorMessage)
        {
            clearError(errorMessage);
            release_stream_resources();
            state.store(CameraState::Stopped, std::memory_order_relaxed);
            return true;
        }

        bool ensure_runtime(std::string *errorMessage){
            if (runtime_acquired) {
                clearError(errorMessage);
                return true;
            }

            if (!IkapSDKRuntime::instance().acquire(errorMessage)) {
                return false;
            }

            runtime_acquired = true;
            return true;
        }

        void release_runtime(){
            if (!runtime_acquired) {
                return;
            }

            IkapSDKRuntime::instance().release();
            runtime_acquired = false;
        }

        // 设置参数
        bool setDouble(const std::string& name, double value){
            if(ItkDevSetDouble(ctx.cameraHandle, name.c_str(), value) != ITKSTATUS_OK){
                return false;
            }
            return true;
        }

        // close camera, 清理资源
        void cleanup()
        {
            release_stream_resources();

            if (ctx.cameraHandle) {
                ItkDevClose(ctx.cameraHandle);
                ctx.resetCameraHandle();
            }

            state.store(CameraState::Stopped, std::memory_order_relaxed);
        }

    };
    

    CameraIkapSdkAdapter::CameraIkapSdkAdapter(CameraInfo info):info_(std::move(info)), impl_(std::make_unique<Impl>(this, info_)){

    }

    CameraIkapSdkAdapter::~CameraIkapSdkAdapter() = default;

    std::vector<CameraInfo> CameraIkapSdkAdapter::enumerateDevices(std::string *errorMessage)
    {
        clearError(errorMessage);
        return {info_};
    }


    bool CameraIkapSdkAdapter::open(std::string *errorMessage){
        const auto state = impl_->state.load(std::memory_order_relaxed);
        if (state == CameraState::Opened) {
            clearError(errorMessage);
            return true;
        }
        const bool ok = impl_->open(errorMessage);
        if(ok){
            LOG_INFO("CameraIkapSdkAdapter open success, logical_name={}", info_.logicalName);
        } else {
            LOG_ERROR("CameraIkapSdkAdapter open failed, logical_name={}", info_.logicalName);
        }
        return ok;
    }

    bool CameraIkapSdkAdapter::close(std::string *errorMessage)
    {
        clearError(errorMessage);

        impl_->cleanup();

        LOG_INFO("CameraIkapSdkAdapter close, logical_name={}", info_.logicalName);
        return true;
    }

    bool CameraIkapSdkAdapter::applyConfig(const CameraConfig &config, std::string *errorMessage)
{
    clearError(errorMessage);

    const auto state = impl_->state.load(std::memory_order_relaxed);

    const bool isIdleLike =
        state == CameraState::Opened ||
        state == CameraState::Configured ||
        state == CameraState::Stopped;

    const bool isStreaming =
        state == CameraState::Streaming;

    if (!isIdleLike && !isStreaming) {
        if (errorMessage) {
            *errorMessage = "camera state does not allow applyConfig";
        }
        LOG_ERROR("CameraIkapSdkAdapter applyConfig rejected, logical_name={}, state={}",
                  info_.logicalName,
                  static_cast<int>(state));
        return false;
    }

    // 运行中只允许改部分参数
    if (isStreaming) {
        const bool wantsTriggerModeChange =
            config.triggerMode != impl_->currentConfig.triggerMode;

        const int newBufferCount = config.bufferCount > 0 ? config.bufferCount : 72;
        const bool wantsBufferCountChange =
            newBufferCount != impl_->bufferCount;

        if (wantsTriggerModeChange || wantsBufferCountChange) {
            if (errorMessage) {
                *errorMessage = "triggerMode/bufferCount cannot be changed while streaming";
            }
            LOG_ERROR("CameraIkapSdkAdapter applyConfig rejected while streaming, logical_name={}, "
                      "triggerModeChange={}, bufferCountChange={}",
                      info_.logicalName,
                      wantsTriggerModeChange,
                      wantsBufferCountChange);
            return false;
        }

        if (config.exposureTime > 0.0) {
            const bool ok = impl_->setDouble("ExposureTime", config.exposureTime);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = "set ExposureTime failed";
                }
                LOG_ERROR("CameraIkapSdkAdapter set exposure time failed while streaming, logical_name={}, value={}",
                          info_.logicalName, config.exposureTime);
                return false;
            }
        }

        if (config.gain > 0.0) {
            const bool ok = impl_->setDouble("DigitalGain", config.gain);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = "set DigitalGain failed";
                }
                LOG_ERROR("CameraIkapSdkAdapter set gain failed while streaming, logical_name={}, value={}",
                          info_.logicalName, config.gain);
                return false;
            }
        }

        // 只更新当前配置里允许在线改的字段
        if (config.exposureTime > 0.0) {
            impl_->currentConfig.exposureTime = config.exposureTime;
        }
        if (config.gain > 0.0) {
            impl_->currentConfig.gain = config.gain;
        }

        clearError(errorMessage);
        LOG_INFO("CameraIkapSdkAdapter applyConfig success while streaming, logical_name={}, exposure={}, gain={}",
                 info_.logicalName,
                 config.exposureTime,
                 config.gain);
        return true;
    }

    // 非 streaming：允许原有全量 apply
    impl_->currentConfig = config;
    impl_->bufferCount = config.bufferCount > 0 ? config.bufferCount : 72;

    if (!impl_->set_trigger_without_grabber(config.triggerMode, errorMessage)) {
        LOG_ERROR("CameraIkapSdkAdapter set trigger mode failed, logical_name={}, value={}",
                  info_.logicalName, static_cast<int>(config.triggerMode));
        return false;
    } else {
        LOG_INFO("CameraIkapSdkAdapter set trigger mode success, logical_name={}, value={}",
                 info_.logicalName, static_cast<int>(config.triggerMode));
    }

    if (config.exposureTime > 0.0) {
        const bool ok = impl_->setDouble("ExposureTime", config.exposureTime);
        if (!ok) {
            if (errorMessage) {
                *errorMessage = "set ExposureTime failed";
            }
            LOG_ERROR("CameraIkapSdkAdapter set exposure time failed, logical_name={}, value={}",
                      info_.logicalName, config.exposureTime);
            return false;
        }
    }

    if (config.gain > 0.0) {
        const bool ok = impl_->setDouble("DigitalGain", config.gain);
        if (!ok) {
            if (errorMessage) {
                *errorMessage = "set DigitalGain failed";
            }
            LOG_ERROR("CameraIkapSdkAdapter set gain failed, logical_name={}, value={}",
                      info_.logicalName, config.gain);
            return false;
        }
    }

    clearError(errorMessage);
    return true;
}

    bool CameraIkapSdkAdapter::startGrabbing(std::string *errorMessage){
        return impl_->start_stream(errorMessage);
    }

    bool CameraIkapSdkAdapter::stopGrabbing(std::string *errorMessage) {
        return impl_->stop_stream(errorMessage);
    }

    bool CameraIkapSdkAdapter::executeSoftwareTrigger(std::string *errorMessage)
    {
        return impl_->software_trigger_grab(errorMessage);
    }


    void CameraIkapSdkAdapter::setFrameCallback(FrameCallback callback){
        impl_->frameCallback = std::move(callback);
    }

    CameraInfo CameraIkapSdkAdapter::info() const{
        return info_;
    }


}
