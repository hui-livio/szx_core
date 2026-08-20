/**
 * @file MagicEnum.hpp
 * @brief   magic_enum 薄封装 — 枚举 ↔ 字符串反射工具（C++17 static reflection）
 * @details 提供编译期枚举元数据查询能力：
 *          - 枚举名 → 字符串
 *          - 枚举值 → 字符串
 *          - 字符串 → 枚举值
 *          - 枚举迭代器
 *
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 *
 * @see     https://github.com/Neargye/magic_enum
 */

#pragma once

#include "magic_enum/magic_enum.hpp"

namespace SZX::COMMON
{

// 枚举反射工具命名空间 — 提供枚举名、枚举值、字符串互转能力
// https://github.com/Neargye/magic_enum
namespace MagicEnum = magic_enum;

} // namespace SZX::COMMON
