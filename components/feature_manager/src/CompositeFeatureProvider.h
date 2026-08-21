/**
 * @file CompositeFeatureProvider.h
 * @brief 多来源 Feature 合并 Provider 声明。
 */

#pragma once

#include "FeatureProvider.h"

#include <memory>
#include <queue>
#include <vector>

namespace SZX::FEATURE
{
class CompositeFeatureProvider final : public IFeatureProvider
{
public:
    explicit CompositeFeatureProvider(std::unique_ptr<IFeatureProvider> localProvider);
    void addSource(std::string sourceId, std::unique_ptr<IFeatureProvider> provider, int priority);

    [[nodiscard]] FeatureLoadResult load() override;
    FeaturePersistResult save(const FeatureMap &features) override;
    [[nodiscard]] FeatureProviderStatus status() const override;

private:
    struct Source
    {
        std::string id;
        int priority;
        std::size_t sequence;
        std::unique_ptr<IFeatureProvider> provider;
        FeatureMap lastSnapshot;
        std::string lastError;
    };

    using SourcePtr = std::shared_ptr<Source>;
    struct LowerPriorityFirst
    {
        bool operator()(const SourcePtr &left, const SourcePtr &right) const
        {
            if (left->priority != right->priority)
                return left->priority > right->priority;
            return left->sequence > right->sequence;
        }
    };

    [[nodiscard]] std::vector<SourcePtr> orderedSources() const;
    [[nodiscard]] FeatureMap mergeWithRemote(FeatureMap local, const std::vector<SourcePtr> &sources) const;
    [[nodiscard]] FeatureMap persistentSnapshot(const FeatureMap &effective, const FeatureMap &localBase,
                                                const std::vector<SourcePtr> &sources) const;
    [[nodiscard]] FeatureOperationResult restoreSnapshots(const FeatureMap &stored);

    std::unique_ptr<IFeatureProvider> m_localProvider;
    std::priority_queue<SourcePtr, std::vector<SourcePtr>, LowerPriorityFirst> m_sources;
    std::size_t m_nextSequence = 0;
    FeatureMap m_localBase;
};
} // namespace SZX::FEATURE
