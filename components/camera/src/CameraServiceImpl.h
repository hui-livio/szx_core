/**
 * @file CameraServiceImpl.h
 * @brief 相机服务实现类 — 提供模块级相机服务门面。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraManager.h"
#include "CameraSdkAdapter.h"
#include "CameraService.h"
#include "CapturePipeline.h"


#include <memory>


namespace SZXCamera
{

struct CameraService::Impl
{
    std::shared_ptr<CameraManager> manager;
    std::shared_ptr<CapturePipeline> pipeline;

    explicit Impl(const CameraServiceConfig &config);
    void bindFrameHandlers();

    static std::shared_ptr<ICameraSdkFactory> createFactory(const CameraServiceConfig &config);
};

} // namespace SZXCamera
