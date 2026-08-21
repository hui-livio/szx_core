/**
 * @file FeatureProvider.h
 * @brief Feature 配置 Provider 的持久化契约与工厂函数。
 */

#pragma once

#include "FeatureValue.h"
#include "Result.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace SZX::FEATURE
{
using FeatureLoadResult      = SZX::COMMON::Result<FeatureMap>;
using FeaturePersistResult   = SZX::COMMON::Result<FeatureMap>;
using FeatureOperationResult = SZX::COMMON::Result<void>;

/** @brief Provider 当前健康状态与非致命告警。 */
struct FeatureProviderStatus
{
    bool stale = false;
    std::vector<std::string> warnings;
};

/** @brief 可加载、保存并报告健康状态的 Feature 数据源接口。 */
class IFeatureProvider
{
public:
    virtual ~IFeatureProvider() = default;

    /** @brief 读取全部 Feature；失败通过 Result 返回。 */
    [[nodiscard]] virtual FeatureLoadResult load()                = 0;
    virtual FeaturePersistResult save(const FeatureMap &features) = 0;
    [[nodiscard]] virtual FeatureProviderStatus status() const
    {
        return {};
    }
};

/** @brief 本地 Feature 文件格式。 */
enum class FeatureFileFormat
{
    Auto,
    Json,
    Yaml,
};

/** @brief 组合 Provider 中一个带稳定来源标识和优先级的来源。 */
struct RankedFeatureProvider
{
    /// 跨进程稳定且在组合 Provider 内唯一，用于恢复该来源的离线缓存。
    std::string sourceId;
    std::unique_ptr<IFeatureProvider> provider;
    int priority = 0;
};

/**
 * @brief 创建本地文件 Provider。
 * @param filePath Feature 文件路径。
 * @param format 文件格式；Auto 根据扩展名推断。
 * @return 调用方拥有的 Provider。
 */
[[nodiscard]] std::unique_ptr<IFeatureProvider> makeLocalFileFeatureProvider(std::filesystem::path filePath,
                                                                             FeatureFileFormat format = FeatureFileFormat::Auto);

/**
 * 创建本地缓存与可选远端源的组合 Provider。高 priority 覆盖低 priority，
 * 最终有效配置会同步回本地。远端实现只需实现 IFeatureProvider。
 * 每个来源必须提供跨重启稳定且唯一的 sourceId。
 */
[[nodiscard]] std::unique_ptr<IFeatureProvider> makeCompositeFeatureProvider(std::unique_ptr<IFeatureProvider> localProvider,
                                                                             std::vector<RankedFeatureProvider> sources = {});
} // namespace SZX::FEATURE
