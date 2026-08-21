/**
 * @file FeatureManager.h
 * @brief 线程安全的 Feature 读取、修改、重载与订阅门面。
 */

#pragma once

#include "FeatureAccess.h"
#include "FeatureProvider.h"

#include <memory>
#include <string>

namespace SZX::FEATURE
{
/** @brief 一次 Provider 提交后的 revision 与 Provider 状态。 */
struct FeatureCommit
{
    std::uint64_t revision = 0;
    FeatureProviderStatus providerStatus;
};

using FeatureCommitResult = SZX::COMMON::Result<FeatureCommit>;

/** @brief 协调 Provider、内存快照与订阅者的 Feature 管理器。 */
class FeatureManager final : public IFeatureAccess
{
public:
    using ChangeCallback = FeatureChangeCallback;
    using SubscriptionId = FeatureSubscriptionId;

    FeatureManager();
    ~FeatureManager() override;
    FeatureManager(const FeatureManager &)            = delete;
    FeatureManager &operator=(const FeatureManager &) = delete;

    /**
     * @brief 替换 Provider 并加载其有效配置。
     * @param provider 新 Provider，成功后由管理器持有。
     * @return 新 revision 或失败原因。
     */
    [[nodiscard]] FeatureCommitResult setProvider(std::unique_ptr<IFeatureProvider> provider);

    [[nodiscard]] bool isEnabled(const std::string &featureName, bool defaultValue = false) const override;
    [[nodiscard]] FeatureValue value(const std::string &featureName, FeatureValue defaultValue = {}) const override;
    [[nodiscard]] std::optional<FeatureValue> findValue(const std::string &featureName) const override;
    [[nodiscard]] FeatureSnapshot snapshot() const override;
    /** @brief 更新并持久化一个布尔 Feature。 */
    [[nodiscard]] FeatureOperationResult setEnabled(const std::string &featureName, bool enabled);
    /** @brief 更新并持久化一个 Feature 值。 */
    [[nodiscard]] FeatureOperationResult setValue(const std::string &featureName, FeatureValue value);
    /** @brief 从当前 Provider 重载有效配置并通知订阅者。 */
    [[nodiscard]] FeatureCommitResult reload();
    [[nodiscard]] FeatureProviderStatus providerStatus() const;

    [[nodiscard("keep the returned subscription alive to receive changes")]]
    FeatureSubscription subscribe(ChangeCallback onChanged) override;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};
} // namespace SZX::FEATURE
