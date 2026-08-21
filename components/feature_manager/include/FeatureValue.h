/**
 * @file FeatureValue.h
 * @brief Feature 配置项的基础值类型与名称映射。
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

namespace SZX::FEATURE
{
using FeatureValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>; ///< 支持持久化的 Feature 值。
using FeatureMap   = std::unordered_map<std::string, FeatureValue>;                         ///< 以稳定名称索引的 Feature 集合。
} // namespace SZX::FEATURE
