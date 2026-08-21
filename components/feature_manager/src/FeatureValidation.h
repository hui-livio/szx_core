/**
 * @file FeatureValidation.h
 * @brief Feature 值类型和持久化数据校验辅助声明。
 */

#pragma once

#include "FeatureProvider.h"

#include <cmath>
#include <cstddef>

namespace SZX::FEATURE::DETAIL
{
inline constexpr std::size_t kMaximumFeatures   = 10000;
inline constexpr std::size_t kMaximumNameBytes  = 1024;
inline constexpr std::size_t kMaximumValueBytes = 1024U * 1024U;

inline FeatureOperationResult validateFeature(const std::string &name, const FeatureValue &value)
{
    constexpr int kValidationError = 4;
    if (name.empty())
        return FeatureOperationResult::Failure(kValidationError, "feature name must not be empty");
    if (name.size() > kMaximumNameBytes)
        return FeatureOperationResult::Failure(kValidationError, "feature name exceeds size limit: " + name);
    if (const auto *text = std::get_if<std::string>(&value); text && text->size() > kMaximumValueBytes)
        return FeatureOperationResult::Failure(kValidationError, "feature value exceeds size limit: " + name);
    if (const auto *number = std::get_if<double>(&value); number && !std::isfinite(*number))
        return FeatureOperationResult::Failure(kValidationError, "feature number must be finite: " + name);
    return FeatureOperationResult::Success();
}

inline FeatureOperationResult validateFeatures(const FeatureMap &features)
{
    constexpr int kValidationError = 4;
    if (features.size() > kMaximumFeatures)
        return FeatureOperationResult::Failure(kValidationError, "feature count exceeds limit");
    for (const auto &[name, value] : features)
    {
        auto result = validateFeature(name, value);
        if (!result.ok)
            return result;
    }
    return FeatureOperationResult::Success();
}
} // namespace SZX::FEATURE::DETAIL
