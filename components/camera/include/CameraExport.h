/**
 * @file CameraExport.h
 * @brief 相机模块导出宏 — 统一 Windows 和非 Windows 符号可见性。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

// camera 模块仅支持动态库，不允许静态库和源码编译

#if defined(_WIN32) || defined(WIN64)
    #ifdef AOI_CAMERA_EXPORTS
        #define AOI_CAMERA_API __declspec(dllexport)
    #else
        #define AOI_CAMERA_API __declspec(dllimport)
    #endif
#else
    #define AOI_CAMERA_API __attribute__((visibility("default")))
#endif
