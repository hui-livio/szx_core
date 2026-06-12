// szx_core example: discover Huaray USB / GigE direct-connect cameras and stream a few frames.
// Built only when Huaray SDK is detected at configure time.

#include "camera/core/camera_service.h"
#include "camera/huaray/camera_huaray_usb_adapter.h"
#include "common/log/log_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

using namespace SZXCamera;

namespace {
class CountSink final : public IFrameSink {
public:
    void consume(const FramePtr &) override { ++count_; }
    std::uint64_t value() const { return count_.load(); }
private:
    std::atomic<std::uint64_t> count_{0};
};
} // namespace

int main()
{
    LogOptions logOpts{};
    logOpts.loggerName = "szx_core_huaray_usb";
    logOpts.consoleEnabled = true;
    std::string err;
    LogManager::instance().init(logOpts, &err);

    auto factory = std::make_shared<HuarayUsbCameraSdkFactory>();
    auto service = std::make_shared<CameraService>(factory);

    if (!service->discover(&err)) {
        std::cerr << "discover failed: " << err << std::endl;
        return 1;
    }
    const auto cameras = service->cameras();
    if (cameras.empty()) {
        std::cerr << "no Huaray USB/GigE cameras online" << std::endl;
        return 1;
    }
    for (const auto &c : cameras) {
        std::cout << "  serial=" << c.serialNumber
                  << " model=" << c.model
                  << " vendor=" << c.vendor << std::endl;
    }
    service->initialize(&err);

    std::unordered_map<std::string, CameraConfig> configs;
    for (const auto &c : cameras) {
        CameraConfig cfg{};
        cfg.exposureTime = 1000.0;
        cfg.gain = 1.0; // adapter will skip if camera has no Gain feature
        cfg.triggerMode = TriggerMode::Continuous;
        cfg.bufferCount = 8;
        configs.emplace(c.serialNumber, cfg);
    }
    if (!service->applyCameraConfigs(configs, &err)) {
        std::cerr << "applyCameraConfigs failed: " << err << std::endl;
        return 1;
    }

    auto sink = std::make_shared<CountSink>();
    service->addFrameSink(sink);

    std::vector<std::string> serials;
    for (const auto &c : cameras) serials.push_back(c.serialNumber);
    service->startCamerasBySerialNumbers(serials, &err);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    service->stopCamerasBySerialNumbers(serials, nullptr);
    service->shutdown(nullptr);

    std::cout << "Captured " << sink->value() << " frames." << std::endl;
    LogManager::instance().shutdown();
    return 0;
}
