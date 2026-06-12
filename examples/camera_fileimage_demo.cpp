// szx_core example: discover and stream from the FileImage fake camera.
// Run without any vendor SDK installed.
//
// Usage:
//   camera_fileimage_demo <image_root_dir>
//
// image_root_dir should contain subdirs camera_0/, camera_1/, ... each with image files.
// The fake camera will replay images at the configured FPS.

#include "camera/core/camera_service.h"
#include "camera/filecamera/camera_fileimage_sdk_adapter.h"
#include "common/log/log_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

using namespace SZXCamera;

namespace {

class StdoutFrameSink final : public IFrameSink
{
public:
    void consume(const FramePtr &frame) override
    {
        if (!frame) return;
        const auto count = ++received_;
        if (count <= 5 || count % 20 == 0) {
            std::cout << "[sink] frame received"
                      << " camera=" << frame->meta.cameraName
                      << " frame_id=" << frame->meta.frameId
                      << " " << frame->width << "x" << frame->height
                      << " ch=" << frame->channels
                      << " size=" << frame->data.size() << " bytes"
                      << " (total=" << count << ")"
                      << std::endl;
        }
    }
    std::uint64_t total() const { return received_.load(); }
private:
    std::atomic<std::uint64_t> received_{0};
};

} // namespace

int main(int argc, char *argv[])
{
    const std::string imageRoot = argc > 1 ? argv[1] : "camera_images";

    LogOptions logOpts{};
    logOpts.loggerName = "szx_core_example";
    logOpts.consoleEnabled = true;
    std::string err;
    if (!LogManager::instance().init(logOpts, &err)) {
        std::cerr << "log init failed: " << err << std::endl;
        return 1;
    }

    auto factory = std::make_shared<FileImageCameraSdkFactory>(
        imageRoot,
        /*fps=*/5,
        /*loop=*/true,
        /*maxFrames=*/0
    );
    auto service = std::make_shared<CameraService>(factory);

    if (!service->discover(&err)) {
        std::cerr << "discover failed: " << err << std::endl;
        return 1;
    }

    const auto cameras = service->cameras();
    if (cameras.empty()) {
        std::cerr << "no cameras discovered under " << imageRoot << std::endl;
        std::cerr << "Hint: create subdirs " << imageRoot
                  << "/camera_0/ etc. with some .png/.jpg files." << std::endl;
        return 1;
    }
    std::cout << "Discovered " << cameras.size() << " cameras:" << std::endl;
    for (const auto &c : cameras) {
        std::cout << "  serial=" << c.serialNumber
                  << " logical=" << c.logicalName
                  << " model=" << c.model
                  << std::endl;
    }

    if (!service->initialize(&err)) {
        std::cerr << "initialize failed: " << err << std::endl;
        return 1;
    }

    std::unordered_map<std::string, CameraConfig> configs;
    for (const auto &c : cameras) {
        CameraConfig cfg{};
        cfg.triggerMode = TriggerMode::Continuous;
        cfg.bufferCount = 8;
        configs.emplace(c.serialNumber, cfg);
    }
    if (!service->applyCameraConfigs(configs, &err)) {
        std::cerr << "applyCameraConfigs failed: " << err << std::endl;
        return 1;
    }

    auto sink = std::make_shared<StdoutFrameSink>();
    service->addFrameSink(sink);

    std::vector<std::string> serials;
    for (const auto &c : cameras) {
        serials.push_back(c.serialNumber);
    }
    if (!service->startCamerasBySerialNumbers(serials, &err)) {
        std::cerr << "start failed: " << err << std::endl;
        return 1;
    }

    std::cout << "Streaming for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    service->stopCamerasBySerialNumbers(serials, nullptr);
    service->shutdown(nullptr);

    std::cout << "Done. Sink received " << sink->total() << " frames." << std::endl;

    LogManager::instance().shutdown();
    return 0;
}
