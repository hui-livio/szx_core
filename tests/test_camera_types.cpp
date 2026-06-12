// Smoke test: verify camera_types.h compiles and basic defaults are sane.

#include "camera/core/camera_types.h"

#include <cassert>
#include <iostream>

using namespace SZXCamera;

int main()
{
    CameraInfo info;
    assert(info.role == CameraRole::Unknown);
    assert(info.serialNumber.empty());

    CameraConfig cfg;
    assert(cfg.triggerMode == TriggerMode::Continuous);
    assert(cfg.pixelFormat == PixelFormat::Unknown);
    assert(cfg.bufferCount == 8);
    assert(cfg.reverseX == false);
    assert(cfg.reverseY == false);

    CameraRuntimeState st;
    assert(st.state == CameraState::Created);
    assert(st.totalFrames == 0);

    FramePacket pkt;
    assert(pkt.width == 0);
    assert(pkt.height == 0);
    assert(pkt.channels == 0);
    assert(pkt.pixelFormat == PixelFormat::Unknown);
    assert(pkt.data.empty());

    FrameMeta meta;
    assert(meta.frameId == 0);
    assert(meta.height == 0.0);
    assert(meta.highExposure == false);

    std::cout << "[OK] test_camera_types" << std::endl;
    return 0;
}
