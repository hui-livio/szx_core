/**
 * @file FeatureManager.cc
 * @brief Feature 快照提交、订阅和持久化协调实现。
 */

#include "FeatureManager.h"
#include "FeatureValidation.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SZX::FEATURE
{
namespace
{
constexpr int kProviderError                 = 1;
constexpr int kInvalidProviderResult         = 2;
thread_local const void *g_currentSubscriber = nullptr;

template <typename SubscriberT, typename Callback>
void invokeSubscriber(const std::shared_ptr<SubscriberT> &subscriber, Callback callback)
{
    {
        std::scoped_lock lock(subscriber->mutex);
        if (!subscriber->active)
            return;
        ++subscriber->inFlight;
    }

    const void *previous = g_currentSubscriber;
    g_currentSubscriber  = subscriber.get();
    try
    {
        callback();
    } catch (...)
    {
        // Observers cannot roll back an already committed update.
    }
    g_currentSubscriber = previous;

    {
        std::scoped_lock lock(subscriber->mutex);
        --subscriber->inFlight;
        if (subscriber->inFlight == 0)
            subscriber->idle.notify_all();
    }
}

FeatureLoadResult safeLoad(const std::shared_ptr<IFeatureProvider> &provider)
{
    try
    {
        return provider->load();
    } catch (const std::exception &error)
    {
        return FeatureLoadResult::Failure(kProviderError, error.what());
    } catch (...)
    {
        return FeatureLoadResult::Failure(kProviderError, "feature provider load failed with an unknown exception");
    }
}

FeaturePersistResult safeSave(const std::shared_ptr<IFeatureProvider> &provider, const FeatureMap &features)
{
    try
    {
        return provider->save(features);
    } catch (const std::exception &error)
    {
        return FeaturePersistResult::Failure(kProviderError, error.what());
    } catch (...)
    {
        return FeaturePersistResult::Failure(kProviderError, "feature provider save failed with an unknown exception");
    }
}

FeatureProviderStatus safeStatus(const std::shared_ptr<IFeatureProvider> &provider)
{
    try
    {
        return provider ? provider->status() : FeatureProviderStatus{};
    } catch (const std::exception &error)
    {
        return {true, {std::string{"feature provider status failed: "} + error.what()}};
    } catch (...)
    {
        return {true, {"feature provider status failed with an unknown exception"}};
    }
}
} // namespace

struct FeatureManager::Impl
{
    struct Subscriber
    {
        std::mutex mutex;
        std::condition_variable idle;
        ChangeCallback onChanged;
        std::size_t inFlight = 0;
        bool active          = true;
    };

    using SubscriberList = std::vector<std::shared_ptr<Subscriber>>;

    struct Event
    {
        SubscriberList listeners;
        std::vector<FeatureChange> changes;
    };

    [[nodiscard]] SubscriberList listenersLocked() const
    {
        SubscriberList result;
        result.reserve(subscribers.size());
        for (const auto &[_, subscriber] : subscribers)
            result.push_back(subscriber);
        return result;
    }

    [[nodiscard]] std::vector<FeatureChange> changes(const FeatureMap &before, const FeatureMap &after,
                                                     std::uint64_t eventRevision) const
    {
        std::vector<FeatureChange> result;
        result.reserve(before.size() + after.size());
        for (const auto &[name, oldFeature] : before)
        {
            const auto newValue = after.find(name);
            if (newValue == after.end())
                result.push_back({name, FeatureChangeKind::Removed, {}, eventRevision});
            else if (oldFeature != newValue->second)
                result.push_back({name, FeatureChangeKind::Updated, newValue->second, eventRevision});
        }
        for (const auto &[name, newFeature] : after)
            if (!before.contains(name))
                result.push_back({name, FeatureChangeKind::Added, newFeature, eventRevision});
        std::ranges::sort(result, {}, &FeatureChange::name);
        return result;
    }

    [[nodiscard]] bool enqueue(Event event)
    {
        std::scoped_lock lock(dispatchMutex);
        events.push_back(std::move(event));
        if (dispatching)
            return false;
        dispatching = true;
        return true;
    }

    void drain()
    {
        for (;;)
        {
            Event current;
            {
                std::scoped_lock lock(dispatchMutex);
                if (events.empty())
                {
                    dispatching = false;
                    return;
                }
                current = std::move(events.front());
                events.pop_front();
            }
            for (const auto &change : current.changes)
                for (const auto &subscriber : current.listeners)
                    invokeSubscriber(subscriber,
                                     [&]
                                     {
                                         if (subscriber->onChanged)
                                             subscriber->onChanged(change);
                                     });
        }
    }

    void unsubscribe(SubscriptionId id)
    {
        std::shared_ptr<Subscriber> subscriber;
        {
            std::scoped_lock lock(stateMutex);
            const auto it = subscribers.find(id);
            if (it == subscribers.end())
                return;
            subscriber = std::move(it->second);
            subscribers.erase(it);
        }

        std::unique_lock lock(subscriber->mutex);
        subscriber->active = false;
        if (g_currentSubscriber != subscriber.get())
            subscriber->idle.wait(lock, [&] { return subscriber->inFlight == 0; });
    }

    mutable std::mutex stateMutex;
    std::mutex ioMutex;
    std::mutex dispatchMutex;
    std::shared_ptr<IFeatureProvider> provider;
    FeatureMap features;
    std::uint64_t revision = 0;
    std::unordered_map<SubscriptionId, std::shared_ptr<Subscriber>> subscribers;
    SubscriptionId nextSubscriptionId = 1;
    std::deque<Event> events;
    bool dispatching = false;
};

FeatureSubscription::FeatureSubscription(FeatureSubscriptionId id, FeatureSnapshot snapshot, std::function<void()> cancel) :
    m_id(id),
    m_snapshot(std::move(snapshot)),
    m_cancel(std::move(cancel))
{
}

FeatureSubscription::FeatureSubscription(FeatureSubscription &&other) noexcept :
    m_id(std::exchange(other.m_id, 0)),
    m_snapshot(std::move(other.m_snapshot)),
    m_cancel(std::move(other.m_cancel))
{
}

FeatureSubscription &FeatureSubscription::operator=(FeatureSubscription &&other) noexcept
{
    if (this != &other)
    {
        reset();
        m_id       = std::exchange(other.m_id, 0);
        m_snapshot = std::move(other.m_snapshot);
        m_cancel   = std::move(other.m_cancel);
    }
    return *this;
}

FeatureSubscription::~FeatureSubscription()
{
    reset();
}

void FeatureSubscription::reset() noexcept
{
    if (m_cancel)
    {
        auto cancel = std::move(m_cancel);
        m_id        = 0;
        try
        {
            cancel();
        } catch (...)
        {
            // Cancellation is best-effort during destruction and must not escape.
        }
    }
}

FeatureManager::FeatureManager() :
    m_impl(std::make_shared<Impl>())
{
}
FeatureManager::~FeatureManager() = default;

FeatureCommitResult FeatureManager::setProvider(std::unique_ptr<IFeatureProvider> provider)
{
    const auto impl = m_impl;
    std::unique_lock ioLock(impl->ioMutex);
    auto newProvider = std::shared_ptr<IFeatureProvider>(std::move(provider));
    FeatureMap loaded;
    if (newProvider)
    {
        auto result = safeLoad(newProvider);
        if (!result.ok)
            return FeatureCommitResult::Failure(result.error, result.message);
        loaded                = std::move(result.value);
        const auto validation = DETAIL::validateFeatures(loaded);
        if (!validation.ok)
            return FeatureCommitResult::Failure(validation.error, validation.message);
    }

    Impl::Event event;
    {
        std::scoped_lock lock(impl->stateMutex);
        auto previous  = std::move(impl->features);
        impl->provider = std::move(newProvider);
        impl->features = std::move(loaded);
        ++impl->revision;
        event.listeners = impl->listenersLocked();
        event.changes   = impl->changes(previous, impl->features, impl->revision);
    }
    std::uint64_t committedRevision = 0;
    std::shared_ptr<IFeatureProvider> committedProvider;
    {
        std::scoped_lock lock(impl->stateMutex);
        committedRevision = impl->revision;
        committedProvider = impl->provider;
    }
    const bool dispatch = impl->enqueue(std::move(event));
    ioLock.unlock();
    const FeatureCommit commit{committedRevision, safeStatus(committedProvider)};
    if (dispatch)
        impl->drain();
    return FeatureCommitResult::Success(commit);
}

bool FeatureManager::isEnabled(const std::string &featureName, bool defaultValue) const
{
    const auto current  = value(featureName, defaultValue);
    const auto *enabled = std::get_if<bool>(&current);
    return enabled ? *enabled : defaultValue;
}

FeatureValue FeatureManager::value(const std::string &featureName, FeatureValue defaultValue) const
{
    const auto found = findValue(featureName);
    return found ? *found : std::move(defaultValue);
}

std::optional<FeatureValue> FeatureManager::findValue(const std::string &featureName) const
{
    const auto impl = m_impl;
    std::scoped_lock lock(impl->stateMutex);
    const auto it = impl->features.find(featureName);
    return it == impl->features.end() ? std::nullopt : std::optional<FeatureValue>{it->second};
}

FeatureSnapshot FeatureManager::snapshot() const
{
    const auto impl = m_impl;
    std::scoped_lock lock(impl->stateMutex);
    return {impl->features, impl->revision};
}

FeatureOperationResult FeatureManager::setEnabled(const std::string &featureName, bool enabled)
{
    return setValue(featureName, enabled);
}

FeatureOperationResult FeatureManager::setValue(const std::string &featureName, FeatureValue newValue)
{
    const auto validation = DETAIL::validateFeature(featureName, newValue);
    if (!validation.ok)
        return validation;
    const auto impl = m_impl;
    std::unique_lock ioLock(impl->ioMutex);
    FeatureMap proposed;
    std::shared_ptr<IFeatureProvider> provider;
    {
        std::scoped_lock lock(impl->stateMutex);
        const auto existing = impl->features.find(featureName);
        if (existing != impl->features.end() && existing->second == newValue)
            return FeatureOperationResult::Success();
        proposed = impl->features;
        proposed.insert_or_assign(featureName, std::move(newValue));
        provider = impl->provider;
    }

    if (provider)
    {
        auto result = safeSave(provider, proposed);
        if (!result.ok)
            return FeatureOperationResult::Failure(result.error, result.message);
        proposed                       = std::move(result.value);
        const auto effectiveValidation = DETAIL::validateFeatures(proposed);
        if (!effectiveValidation.ok)
            return effectiveValidation;
    }
    if (!proposed.contains(featureName))
    {
        const std::string message = "feature provider omitted the requested feature: " + featureName;
        return FeatureOperationResult::Failure(kInvalidProviderResult, message);
    }

    Impl::Event event;
    {
        std::scoped_lock lock(impl->stateMutex);
        auto previous  = std::move(impl->features);
        impl->features = std::move(proposed);
        ++impl->revision;
        event.listeners = impl->listenersLocked();
        event.changes   = impl->changes(previous, impl->features, impl->revision);
    }
    const bool dispatch = impl->enqueue(std::move(event));
    ioLock.unlock();
    if (dispatch)
        impl->drain();
    return FeatureOperationResult::Success();
}

FeatureCommitResult FeatureManager::reload()
{
    const auto impl = m_impl;
    std::unique_lock ioLock(impl->ioMutex);
    std::shared_ptr<IFeatureProvider> provider;
    {
        std::scoped_lock lock(impl->stateMutex);
        provider = impl->provider;
    }

    FeatureMap loaded;
    if (provider)
    {
        auto result = safeLoad(provider);
        if (!result.ok)
            return FeatureCommitResult::Failure(result.error, result.message);
        loaded                = std::move(result.value);
        const auto validation = DETAIL::validateFeatures(loaded);
        if (!validation.ok)
            return FeatureCommitResult::Failure(validation.error, validation.message);
    }

    Impl::Event event;
    {
        std::scoped_lock lock(impl->stateMutex);
        auto previous  = std::move(impl->features);
        impl->features = std::move(loaded);
        ++impl->revision;
        event.listeners = impl->listenersLocked();
        event.changes   = impl->changes(previous, impl->features, impl->revision);
    }
    std::uint64_t committedRevision = 0;
    {
        std::scoped_lock lock(impl->stateMutex);
        committedRevision = impl->revision;
    }
    const bool dispatch = impl->enqueue(std::move(event));
    ioLock.unlock();
    const FeatureCommit commit{committedRevision, safeStatus(provider)};
    if (dispatch)
        impl->drain();
    return FeatureCommitResult::Success(commit);
}

FeatureProviderStatus FeatureManager::providerStatus() const
{
    const auto impl = m_impl;
    std::scoped_lock ioLock(impl->ioMutex);
    std::shared_ptr<IFeatureProvider> provider;
    {
        std::scoped_lock lock(impl->stateMutex);
        provider = impl->provider;
    }
    return safeStatus(provider);
}

FeatureSubscription FeatureManager::subscribe(ChangeCallback onChanged)
{
    auto subscriber       = std::make_shared<Impl::Subscriber>();
    subscriber->onChanged = std::move(onChanged);

    const auto impl = m_impl;
    FeatureSnapshot current;
    SubscriptionId id = 0;
    {
        std::scoped_lock lock(impl->stateMutex);
        id = impl->nextSubscriptionId++;
        impl->subscribers.emplace(id, std::move(subscriber));
        current = {impl->features, impl->revision};
    }
    std::weak_ptr<Impl> weak = impl;
    return FeatureSubscription{id, std::move(current), [weak, id]
                               {
                                   if (const auto state = weak.lock())
                                       state->unsubscribe(id);
                               }};
}
} // namespace SZX::FEATURE
