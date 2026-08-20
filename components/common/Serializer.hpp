/**
 * @file Serializer.hpp
 * @brief   序列化薄封装 — 引入 iguana/nlohmann 并建立命名空间别名
 * @details 提供统一的序列化/反序列化接口：
 *          - JSON/YAML 序列化（基于 iguana）
 *          - JSON 解析和动态构建（基于 nlohmann/json）
 *
 * @note    所有类型通过 CMake 的 SZX_REFL 宏自动注册反射信息
 *
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include "iguana/iguana.hpp" // IWYU pragma: keep (薄封装再导出)
#include "nlohmann/json.hpp" // IWYU pragma: keep (薄封装再导出)


// 反射宏别名 — 用于自动生成结构体的序列化反射信息
#define SZX_REFL YLT_REFL

namespace SZX::COMMON
{

// 序列化工具命名空间 — 基于 iguana，提供 JSON/YAML 序列化能力
namespace Serializer = iguana; // NOLINT (薄封装再导出)

// JSON 动态类型 — 基于 nlohmann/json，提供 JSON 解析/构建能力
using Json = nlohmann::json; // NOLINT (薄封装再导出)

} // namespace SZX::COMMON
