/**
 * @file LocalFileFeatureProvider.h
 * @brief 本地 JSON/YAML Feature Provider 声明。
 */

#pragma once

#include "FeatureProvider.h"

#include <filesystem>
#include <mutex>

namespace SZX::FEATURE
{
class LocalFileFeatureProvider final : public IFeatureProvider
{
public:
    enum class Format
    {
        Auto,
        Json,
        Yaml
    };

    explicit LocalFileFeatureProvider(std::filesystem::path filePath, Format format = Format::Auto);
    [[nodiscard]] FeatureLoadResult load() override;
    FeaturePersistResult save(const FeatureMap &features) override;
    [[nodiscard]] FeatureProviderStatus status() const override;

private:
    [[nodiscard]] Format resolvedFormat() const;

    std::filesystem::path m_filePath;
    Format m_format;
    mutable std::mutex m_statusMutex;
    FeatureProviderStatus m_status;
};
} // namespace SZX::FEATURE
