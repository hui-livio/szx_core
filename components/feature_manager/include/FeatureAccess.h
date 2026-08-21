/**
 * @file FeatureAccess.h
 * @brief Feature 只读访问、快照与订阅契约。
 */

#pragma once

#include "FeatureValue.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace SZX::FEATURE
{
/** @brief 一个 Feature 相对上个有效快照的变化类型。 */
enum class FeatureChangeKind
{
    Added,
    Updated,
    Removed,
};

/** @brief 订阅回调收到的单项变更。 */
struct FeatureChange
{
    std::string name;
    FeatureChangeKind kind = FeatureChangeKind::Updated;
    FeatureValue value;
    std::uint64_t revision = 0;
};

/** @brief 指定 revision 下的一致 Feature 快照。 */
struct FeatureSnapshot
{
    FeatureMap values;
    std::uint64_t revision = 0;
};

using FeatureChangeCallback = std::function<void(const FeatureChange &)>;
using FeatureSubscriptionId = std::size_t;

/** @brief RAII 订阅句柄，析构或 reset 时取消订阅。 */
class FeatureSubscription
{
public:
    FeatureSubscription()                                       = default;
    FeatureSubscription(const FeatureSubscription &)            = delete;
    FeatureSubscription &operator=(const FeatureSubscription &) = delete;
    FeatureSubscription(FeatureSubscription &&other) noexcept;
    FeatureSubscription &operator=(FeatureSubscription &&other) noexcept;
    ~FeatureSubscription();

    [[nodiscard]] FeatureSubscriptionId id() const noexcept
    {
        return m_id;
    }
    [[nodiscard]] const FeatureSnapshot &snapshot() const noexcept
    {
        return m_snapshot;
    }
    /** @brief 取消订阅；可重复调用。 */
    void reset() noexcept;

private:
    friend class FeatureManager;
    FeatureSubscription(FeatureSubscriptionId id, FeatureSnapshot snapshot, std::function<void()> cancel);

    FeatureSubscriptionId m_id = 0;
    FeatureSnapshot m_snapshot;
    std::function<void()> m_cancel;
};

/** @brief 业务调用方只依赖此只读接口查询 Feature 和订阅有效状态变化。 */
class IFeatureAccess
{
public:
    virtual ~IFeatureAccess() = default;

    /** @brief 返回布尔 Feature；不存在或类型不匹配时返回 defaultValue。 */
    [[nodiscard]] virtual bool isEnabled(const std::string &featureName, bool defaultValue = false) const          = 0;
    [[nodiscard]] virtual FeatureValue value(const std::string &featureName, FeatureValue defaultValue = {}) const = 0;
    [[nodiscard]] virtual std::optional<FeatureValue> findValue(const std::string &featureName) const              = 0;
    [[nodiscard]] virtual FeatureSnapshot snapshot() const                                                         = 0;
    [[nodiscard("keep the returned subscription alive to receive changes")]]
    virtual FeatureSubscription subscribe(FeatureChangeCallback onChanged) = 0;
};
} // namespace SZX::FEATURE
