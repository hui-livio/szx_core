/**
 * @file IFrameSink.h
 * @brief 帧接收接口 — 定义相机帧回调消费端。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once
#include "CameraExport.h"
#include "CameraTypes.h"

#include <memory>

namespace SZXCamera
{

class AOI_CAMERA_API IFrameSink
{
public:
    virtual ~IFrameSink()                                                 = default;
    virtual void consume(const std::shared_ptr<const FramePacket> &frame) = 0;
};

} // namespace SZXCamera
