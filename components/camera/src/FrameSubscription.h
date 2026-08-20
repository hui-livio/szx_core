/**
 * @file FrameSubscription.h
 * @brief 帧订阅辅助类型 — 管理相机帧回调订阅生命周期。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "CameraTypes.h"

#include <string>
#include <vector>


namespace SZXCamera
{

/// @brief 帧订阅（内部类，不导出）
struct FrameSubscription
{
    std::vector<std::string> cameraSerialNumbers;
    std::vector<std::string> cameraNames;
    std::vector<std::string> taskIds;
    bool onlyHighExposure{false};

    bool empty() const
    {
        return cameraSerialNumbers.empty() && cameraNames.empty() && taskIds.empty() && !onlyHighExposure;
    }
};

} // namespace SZXCamera
