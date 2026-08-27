/**
 * @file NativeWindow.h
 * @brief Windows 原生窗口辅助 — 安装无边框窗口事件过滤并配置 DWM 效果。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "UIToolkitsExport.h"

#include <QWindow>

// 安装原生事件过滤器（NCCALCSIZE 处理），必须在创建窗口前调用一次。
UITOOLKITS_API void installNativeWindowFilter();

// 为现有 QWindow 应用 DWM 阴影和深色标题栏。
UITOOLKITS_API void setupNativeWindow(QWindow *win);

// 在窗口完成 show() 后解蔽 DWM cloak，避免首次显示短暂出现系统标题栏。
UITOOLKITS_API void revealNativeWindow(QWindow *win);
