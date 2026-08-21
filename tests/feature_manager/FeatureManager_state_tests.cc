/**
 * @file FeatureManager_state_tests.cc
 * @brief FeatureManager_state_tests.cc 覆盖的单元测试与测试辅助。
 */

#include "FeatureManager_test_support.h"

namespace
{
TEST(FeatureManagerTest, FailedReloadKeepsLastKnownGoodSnapshot)
{
    auto provider        = std::make_unique<MemoryProvider>();
    auto *providerAccess = provider.get();
    provider->values.emplace("enabled", true);

    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::move(provider)).ok);
    EXPECT_TRUE(manager.isEnabled("enabled"));

    providerAccess->failLoad = true;
    const auto result        = manager.reload();

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(manager.isEnabled("enabled"));
}

TEST(FeatureManagerTest, FailedSaveDoesNotPublishOrCommitValue)
{
    auto provider        = std::make_unique<MemoryProvider>();
    auto *providerAccess = provider.get();
    provider->values.emplace("enabled", false);

    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::move(provider)).ok);
    int notifications = 0;
    auto subscription = manager.subscribe([&](const FeatureChange &) { ++notifications; });

    providerAccess->failSave = true;
    const auto result        = manager.setEnabled("enabled", true);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(manager.isEnabled("enabled"));
    EXPECT_EQ(notifications, 0);
}

TEST(FeatureManagerTest, ProviderExceptionBecomesFailure)
{
    class ThrowingProvider final : public IFeatureProvider
    {
    public:
        FeatureLoadResult load() override
        {
            throw std::runtime_error("provider exploded");
        }
        FeaturePersistResult save(const FeatureMap &features) override
        {
            return FeaturePersistResult::Success(features);
        }
    };

    FeatureManager manager;
    const auto result = manager.setProvider(std::make_unique<ThrowingProvider>());
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("provider exploded"), std::string::npos);
}

TEST(FeatureManagerTest, UnsubscribeWaitsForInFlightCallback)
{
    FeatureManager manager;
    std::mutex mutex;
    std::condition_variable enteredCondition;
    std::condition_variable releaseCondition;
    bool entered = false;
    bool release = false;

    auto subscription = manager.subscribe(
        [&](const FeatureChange &)
        {
            std::unique_lock lock(mutex);
            entered = true;
            enteredCondition.notify_one();
            releaseCondition.wait(lock, [&] { return release; });
        });

    auto writer = std::async(std::launch::async, [&] { return manager.setEnabled("enabled", true); });
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(enteredCondition.wait_for(lock, 1s, [&] { return entered; }));
    }

    auto unsubscribe = std::async(std::launch::async, [&] { subscription.reset(); });
    EXPECT_EQ(unsubscribe.wait_for(30ms), std::future_status::timeout);
    {
        std::scoped_lock lock(mutex);
        release = true;
    }
    releaseCondition.notify_one();

    EXPECT_EQ(unsubscribe.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(writer.get().ok);
}

TEST(FeatureManagerTest, CallbackCanUnsubscribeItself)
{
    FeatureManager manager;
    FeatureSubscription subscription;
    int calls    = 0;
    subscription = manager.subscribe(
        [&](const FeatureChange &)
        {
            ++calls;
            subscription.reset();
        });

    ASSERT_TRUE(manager.setEnabled("first", true).ok);
    ASSERT_TRUE(manager.setEnabled("second", true).ok);
    EXPECT_EQ(calls, 1);
}

TEST(FeatureManagerTest, ReloadPublishesOnlyChangedEffectiveFeatures)
{
    auto provider        = std::make_unique<MemoryProvider>();
    auto *providerAccess = provider.get();
    provider->values     = {{"same", true}, {"changed", false}};

    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::move(provider)).ok);
    std::vector<std::string> changedNames;
    auto subscription = manager.subscribe([&](const FeatureChange &change) { changedNames.push_back(change.name); });

    providerAccess->values = {{"same", true}, {"changed", true}, {"added", std::int64_t{7}}};
    ASSERT_TRUE(manager.reload().ok);

    EXPECT_EQ(changedNames, (std::vector<std::string>{"added", "changed"}));
}

TEST(FeatureManagerTest, SubscriptionAtomicallyReturnsCurrentSnapshot)
{
    FeatureManager manager;
    ASSERT_TRUE(manager.setEnabled("ready", true).ok);

    const auto subscription = manager.subscribe([](const FeatureChange &) {});

    EXPECT_TRUE(std::get<bool>(subscription.snapshot().values.at("ready")));
    EXPECT_EQ(subscription.snapshot().revision, manager.snapshot().revision);
}

TEST(FeatureManagerTest, RemovalIsDistinctFromNullValue)
{
    auto provider        = std::make_unique<MemoryProvider>();
    auto *providerAccess = provider.get();
    provider->values     = {{"removed", true}, {"nullValue", true}};
    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::move(provider)).ok);

    std::vector<FeatureChange> changes;
    auto subscription      = manager.subscribe([&](const FeatureChange &change) { changes.push_back(change); });
    providerAccess->values = {{"nullValue", std::monostate{}}};
    ASSERT_TRUE(manager.reload().ok);

    ASSERT_EQ(changes.size(), 2);
    EXPECT_EQ(changes[0].kind, FeatureChangeKind::Updated);
    EXPECT_EQ(changes[1].kind, FeatureChangeKind::Removed);
}

TEST(FeatureManagerTest, ProviderEffectiveMapPublishesEveryChangedFeature)
{
    class NormalizingProvider final : public MemoryProvider
    {
    public:
        FeaturePersistResult save(const FeatureMap &features) override
        {
            values = features;
            values.insert_or_assign("normalized", true);
            return FeaturePersistResult::Success(values);
        }
    };

    auto provider = std::make_unique<NormalizingProvider>();
    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::move(provider)).ok);
    std::vector<std::string> names;
    auto subscription = manager.subscribe([&](const FeatureChange &change) { names.push_back(change.name); });

    ASSERT_TRUE(manager.setEnabled("requested", true).ok);
    EXPECT_EQ(names, (std::vector<std::string>{"normalized", "requested"}));
}

TEST(FeatureManagerTest, ProviderCannotCommitMapMissingRequestedFeature)
{
    class OmittingProvider final : public MemoryProvider
    {
    public:
        FeaturePersistResult save(const FeatureMap &) override
        {
            return FeaturePersistResult::Success({{"other", true}});
        }
    };

    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::make_unique<OmittingProvider>()).ok);
    const auto result = manager.setEnabled("requested", true);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(manager.isEnabled("requested"));
}

TEST(FeatureManagerTest, ReentrantUpdatesAreDeliveredInCommitOrder)
{
    FeatureManager manager;
    std::vector<std::string> secondSubscriberOrder;
    auto firstSubscription = manager.subscribe(
        [&](const FeatureChange &change)
        {
            if (change.name == "first")
                EXPECT_TRUE(manager.setEnabled("second", true).ok);
        });
    auto secondSubscription =
        manager.subscribe([&](const FeatureChange &change) { secondSubscriberOrder.push_back(change.name); });

    ASSERT_TRUE(manager.setEnabled("first", true).ok);
    EXPECT_EQ(secondSubscriberOrder, (std::vector<std::string>{"first", "second"}));
}

TEST(FeatureManagerTest, ScopedSubscriptionAutomaticallyDisconnects)
{
    FeatureManager manager;
    int calls = 0;
    {
        auto subscription = manager.subscribe([&](const FeatureChange &) { ++calls; });
        EXPECT_TRUE(manager.setEnabled("first", true).ok);
    }
    EXPECT_TRUE(manager.setEnabled("second", true).ok);
    EXPECT_EQ(calls, 1);
}

TEST(FeatureManagerTest, MoveConstructionTransfersSubscriptionIdentity)
{
    FeatureManager manager;
    auto original = manager.subscribe([](const FeatureChange &) {});
    const auto id = original.id();

    FeatureSubscription moved(std::move(original));

    EXPECT_EQ(original.id(), 0);
    EXPECT_EQ(moved.id(), id);
}

TEST(FeatureManagerTest, CommitResultUsesTheCommittedRevisionAndProviderStatus)
{
    auto provider            = std::make_unique<MemoryProvider>();
    provider->providerStatus = {true, {"cached remote configuration"}};
    FeatureManager manager;

    const auto installed = manager.setProvider(std::move(provider));

    ASSERT_TRUE(installed.ok);
    EXPECT_EQ(installed.value.revision, 1);
    EXPECT_TRUE(installed.value.providerStatus.stale);
    ASSERT_EQ(installed.value.providerStatus.warnings.size(), 1);
    EXPECT_EQ(installed.value.providerStatus.warnings.front(), "cached remote configuration");
}

TEST(FeatureManagerTest, CallbackDoesNotHoldProviderIoLock)
{
    FeatureManager manager;
    std::future<FeatureOperationResult> nested;
    auto subscription = manager.subscribe(
        [&](const FeatureChange &change)
        {
            if (change.name != "first")
                return;
            nested = std::async(std::launch::async, [&] { return manager.setEnabled("second", true); });
            EXPECT_EQ(nested.wait_for(1s), std::future_status::ready);
        });

    EXPECT_TRUE(manager.setEnabled("first", true).ok);
    EXPECT_TRUE(nested.valid());
    EXPECT_TRUE(nested.get().ok);
}

TEST(FeatureManagerTest, RejectsValuesThatCannotRoundTrip)
{
    FeatureManager manager;
    EXPECT_FALSE(manager.setEnabled("", true).ok);
    EXPECT_FALSE(manager.setValue("nan", std::numeric_limits<double>::quiet_NaN()).ok);
    EXPECT_FALSE(manager.setValue("infinity", std::numeric_limits<double>::infinity()).ok);
}

TEST(FeatureManagerTest, ReadsRemainAvailableWhileProviderSaveIsBlocked)
{
    class BlockingProvider final : public IFeatureProvider
    {
    public:
        FeatureLoadResult load() override
        {
            return FeatureLoadResult::Success({{"existing", true}});
        }

        FeaturePersistResult save(const FeatureMap &features) override
        {
            std::unique_lock lock(mutex);
            entered = true;
            enteredCondition.notify_one();
            releaseCondition.wait(lock, [&] { return release; });
            return FeaturePersistResult::Success(features);
        }

        std::mutex mutex;
        std::condition_variable enteredCondition;
        std::condition_variable releaseCondition;
        bool entered = false;
        bool release = false;
    };

    auto provider        = std::make_unique<BlockingProvider>();
    auto *providerAccess = provider.get();
    FeatureManager manager;
    ASSERT_TRUE(manager.setProvider(std::move(provider)).ok);

    auto writer = std::async(std::launch::async, [&] { return manager.setEnabled("new", true); });
    {
        std::unique_lock lock(providerAccess->mutex);
        ASSERT_TRUE(providerAccess->enteredCondition.wait_for(lock, 1s, [&] { return providerAccess->entered; }));
    }

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(manager.isEnabled("existing"));
    EXPECT_LT(std::chrono::steady_clock::now() - started, 50ms);

    {
        std::scoped_lock lock(providerAccess->mutex);
        providerAccess->release = true;
    }
    providerAccess->releaseCondition.notify_one();
    EXPECT_TRUE(writer.get().ok);
}


} // namespace
