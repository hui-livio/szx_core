/**
 * @file CompositeFeatureProvider.cc
 * @brief 多来源 Feature 合并与本地回写实现。
 */

#include "CompositeFeatureProvider.h"
#include "FeatureValidation.h"

#include <algorithm>
#include <charconv>
#include <exception>
#include <stdexcept>
#include <string_view>

namespace SZX::FEATURE
{
namespace
{
constexpr int kCompositeError               = 3;
constexpr std::string_view kMetadataMarker  = "__szx_feature_manager_v1";
constexpr std::string_view kBasePrefix      = "__szx_feature_manager_v1.base.";
constexpr std::string_view kRemotePrefix    = "__szx_feature_manager_v1.remote.";
constexpr std::string_view kMetadataValue   = "composite-cache-v1";
constexpr std::size_t kMaximumSourceIdBytes = 256;

std::string hexEncodeKey(std::string_view value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (const unsigned char byte : value)
    {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

bool hexDecodeKey(std::string_view value, std::string &result)
{
    if (value.size() % 2 != 0)
        return false;
    const auto nibble = [](char digit) -> int
    {
        if (digit >= '0' && digit <= '9')
            return digit - '0';
        if (digit >= 'a' && digit <= 'f')
            return digit - 'a' + 10;
        if (digit >= 'A' && digit <= 'F')
            return digit - 'A' + 10;
        return -1;
    };
    result.clear();
    result.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2)
    {
        const int high = nibble(value[index]);
        const int low  = nibble(value[index + 1]);
        if (high < 0 || low < 0)
            return false;
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}
} // namespace

CompositeFeatureProvider::CompositeFeatureProvider(std::unique_ptr<IFeatureProvider> localProvider) :
    m_localProvider(std::move(localProvider))
{
}

void CompositeFeatureProvider::addSource(std::string sourceId, std::unique_ptr<IFeatureProvider> provider, int priority)
{
    if (!provider)
        return;
    if (sourceId.empty())
        throw std::invalid_argument("feature source ID must not be empty");
    if (sourceId.size() > kMaximumSourceIdBytes)
        throw std::invalid_argument("feature source ID exceeds size limit");
    auto sources = orderedSources();
    if (std::ranges::any_of(sources, [&sourceId](const SourcePtr &source) { return source->id == sourceId; }))
        throw std::invalid_argument("duplicate feature source ID: " + sourceId);
    m_sources.push(
        std::make_shared<Source>(Source{std::move(sourceId), priority, m_nextSequence++, std::move(provider), {}, {}}));
}

FeatureLoadResult CompositeFeatureProvider::load()
{
    if (!m_localProvider)
        return FeatureLoadResult::Failure(kCompositeError, "composite feature provider requires a local provider");

    auto stored = m_localProvider->load();
    if (!stored.ok)
        return stored;
    const auto restored = restoreSnapshots(stored.value);
    if (!restored.ok)
        return FeatureLoadResult::Failure(restored.error, restored.message);

    const auto sources = orderedSources();
    for (const auto &source : sources)
    {
        try
        {
            auto loaded = source->provider->load();
            if (loaded.ok)
            {
                source->lastSnapshot = std::move(loaded.value);
                source->lastError.clear();
            }
            else
            {
                source->lastError = loaded.message;
            }
        } catch (const std::exception &error)
        {
            source->lastError = error.what();
        } catch (...)
        {
            source->lastError = "remote feature provider failed with an unknown exception";
        }
    }

    auto effective    = mergeWithRemote(m_localBase, sources);
    auto synchronized = m_localProvider->save(persistentSnapshot(effective, m_localBase, sources));
    if (!synchronized.ok)
        return FeatureLoadResult::Failure(synchronized.error, synchronized.message);
    return FeatureLoadResult::Success(std::move(effective));
}

FeaturePersistResult CompositeFeatureProvider::save(const FeatureMap &features)
{
    if (!m_localProvider)
        return FeaturePersistResult::Failure(kCompositeError, "composite feature provider requires a local provider");

    const auto validation = DETAIL::validateFeatures(features);
    if (!validation.ok)
        return FeaturePersistResult::Failure(validation.error, validation.message);
    for (const auto &[name, _] : features)
        if (name.starts_with(kMetadataMarker))
            return FeaturePersistResult::Failure(kCompositeError, "reserved feature name prefix: " + name);

    FeatureMap nextBase = features;
    const auto sources  = orderedSources();
    for (const auto &source : sources)
    {
        for (const auto &[name, _] : source->lastSnapshot)
        {
            const auto previous = m_localBase.find(name);
            if (previous == m_localBase.end())
                nextBase.erase(name);
            else
                nextBase.insert_or_assign(name, previous->second);
        }
    }
    auto effective = mergeWithRemote(nextBase, sources);
    auto persisted = m_localProvider->save(persistentSnapshot(effective, nextBase, sources));
    if (!persisted.ok)
        return FeaturePersistResult::Failure(persisted.error, persisted.message);
    m_localBase = std::move(nextBase);
    return FeaturePersistResult::Success(std::move(effective));
}

FeatureProviderStatus CompositeFeatureProvider::status() const
{
    FeatureProviderStatus result;
    try
    {
        const auto localStatus = m_localProvider ? m_localProvider->status() : FeatureProviderStatus{};
        result.stale           = localStatus.stale;
        for (const auto &warning : localStatus.warnings)
            result.warnings.push_back("local feature cache: " + warning);
    } catch (const std::exception &error)
    {
        result.stale = true;
        result.warnings.push_back(std::string{"local feature cache status failed: "} + error.what());
    } catch (...)
    {
        result.stale = true;
        result.warnings.push_back("local feature cache status failed");
    }
    for (const auto &source : orderedSources())
    {
        if (!source->lastError.empty())
        {
            result.stale = true;
            result.warnings.push_back("feature source '" + source->id + "': " + source->lastError);
        }
        try
        {
            auto childStatus = source->provider->status();
            result.stale     = result.stale || childStatus.stale;
            for (const auto &warning : childStatus.warnings)
                result.warnings.push_back("feature source '" + source->id + "': " + warning);
        } catch (const std::exception &error)
        {
            result.stale = true;
            result.warnings.push_back("feature source '" + source->id + "' status failed: " + error.what());
        } catch (...)
        {
            result.stale = true;
            result.warnings.push_back("feature source '" + source->id + "' status failed");
        }
    }
    return result;
}

FeatureMap CompositeFeatureProvider::mergeWithRemote(FeatureMap local, const std::vector<SourcePtr> &sources) const
{
    for (const auto &source : sources)
        for (const auto &[name, value] : source->lastSnapshot)
            local.insert_or_assign(name, value);
    return local;
}

FeatureMap CompositeFeatureProvider::persistentSnapshot(const FeatureMap &effective, const FeatureMap &localBase,
                                                        const std::vector<SourcePtr> &sources) const
{
    FeatureMap result = effective;
    result.insert_or_assign(std::string{kMetadataMarker}, std::string{kMetadataValue});
    for (const auto &[name, value] : localBase)
        result.insert_or_assign(std::string{kBasePrefix} + hexEncodeKey(name), value);
    for (const auto &source : sources)
    {
        const std::string prefix = std::string{kRemotePrefix} + hexEncodeKey(source->id) + ".";
        for (const auto &[name, value] : source->lastSnapshot)
            result.insert_or_assign(prefix + hexEncodeKey(name), value);
    }
    return result;
}

FeatureOperationResult CompositeFeatureProvider::restoreSnapshots(const FeatureMap &stored)
{
    const auto marker          = stored.find(std::string{kMetadataMarker});
    const bool currentMetadata = marker != stored.end() && std::get_if<std::string>(&marker->second) &&
        std::get<std::string>(marker->second) == kMetadataValue;
    const bool hasMetadataEntries = std::ranges::any_of(
        stored, [](const auto &entry) { return entry.first.starts_with(kBasePrefix) || entry.first.starts_with(kRemotePrefix); });
    const bool legacyMetadata =
        marker != stored.end() && std::get_if<std::monostate>(&marker->second) != nullptr && hasMetadataEntries;
    const bool hasMetadata = currentMetadata || legacyMetadata;
    if (!hasMetadata)
    {
        for (const auto &[name, _] : stored)
            if (name.starts_with(kMetadataMarker))
                return FeatureOperationResult::Failure(kCompositeError, "reserved feature name prefix: " + name);
        m_localBase = stored;
        return FeatureOperationResult::Success();
    }

    m_localBase.clear();
    const auto sources = orderedSources();
    for (const auto &source : sources)
        source->lastSnapshot.clear();

    for (const auto &[key, value] : stored)
    {
        std::string name;
        if (key.starts_with(kBasePrefix))
        {
            if (!hexDecodeKey(std::string_view{key}.substr(kBasePrefix.size()), name))
                return FeatureOperationResult::Failure(kCompositeError, "invalid local feature cache metadata");
            m_localBase.insert_or_assign(std::move(name), value);
            continue;
        }
        if (!key.starts_with(kRemotePrefix))
            continue;

        std::string_view suffix = std::string_view{key}.substr(kRemotePrefix.size());
        const auto separator    = suffix.find('.');
        if (separator == std::string_view::npos)
            return FeatureOperationResult::Failure(kCompositeError, "invalid remote feature cache metadata");
        std::string sourceId;
        const bool decodedSource = hexDecodeKey(suffix.substr(0, separator), sourceId);
        if (!hexDecodeKey(suffix.substr(separator + 1), name) || (!decodedSource && !legacyMetadata))
            return FeatureOperationResult::Failure(kCompositeError, "invalid remote feature cache metadata");
        bool restored = false;
        if (decodedSource)
            for (const auto &source : sources)
                if (source->id == sourceId)
                {
                    source->lastSnapshot.insert_or_assign(name, value);
                    restored = true;
                    break;
                }
        if (!restored && legacyMetadata)
        {
            std::size_t sequence = 0;
            const auto parsed    = std::from_chars(suffix.data(), suffix.data() + separator, sequence);
            if (parsed.ec == std::errc{} && parsed.ptr == suffix.data() + separator)
                for (const auto &source : sources)
                    if (source->sequence == sequence)
                        source->lastSnapshot.insert_or_assign(name, value);
        }
    }
    return FeatureOperationResult::Success();
}

std::vector<CompositeFeatureProvider::SourcePtr> CompositeFeatureProvider::orderedSources() const
{
    auto queue = m_sources;
    std::vector<SourcePtr> result;
    result.reserve(queue.size());
    while (!queue.empty())
    {
        result.push_back(queue.top());
        queue.pop();
    }
    return result;
}

std::unique_ptr<IFeatureProvider> makeCompositeFeatureProvider(std::unique_ptr<IFeatureProvider> localProvider,
                                                               std::vector<RankedFeatureProvider> sources)
{
    if (sources.empty())
        return localProvider;
    auto composite = std::make_unique<CompositeFeatureProvider>(std::move(localProvider));
    for (std::size_t index = 0; index < sources.size(); ++index)
    {
        auto source = std::move(sources[index]);
        composite->addSource(std::move(source.sourceId), std::move(source.provider), source.priority);
    }
    return composite;
}
} // namespace SZX::FEATURE
