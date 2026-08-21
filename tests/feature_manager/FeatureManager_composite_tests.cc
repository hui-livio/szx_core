/**
 * @file FeatureManager_composite_tests.cc
 * @brief FeatureManager_composite_tests.cc 覆盖的单元测试与测试辅助。
 */

#include "FeatureManager_test_support.h"

namespace
{
TEST(CompositeFeatureProviderTest, RemoteWinsAndEffectiveSnapshotIsSynchronizedLocally)
{
    auto local        = std::make_unique<MemoryProvider>();
    auto *localAccess = local.get();
    local->values     = {{"localOnly", true}, {"conflict", std::string{"local"}}};

    auto remote    = std::make_unique<MemoryProvider>();
    remote->values = {{"remoteOnly", std::int64_t{9}}, {"conflict", std::string{"remote"}}};

    CompositeFeatureProvider composite(std::move(local));
    composite.addSource("remote", std::move(remote), 100);
    const auto loaded = composite.load();

    ASSERT_TRUE(loaded.ok) << loaded.message;
    EXPECT_EQ(std::get<std::string>(loaded.value.at("conflict")), "remote");
    EXPECT_EQ(std::get<std::string>(localAccess->values.at("conflict")), "remote");
    EXPECT_EQ(std::get<std::int64_t>(localAccess->values.at("remoteOnly")), 9);
}

TEST(CompositeFeatureProviderTest, HighestPrioritySourceWins)
{
    auto local    = std::make_unique<MemoryProvider>();
    local->values = {{"conflict", std::string{"local"}}};
    auto high     = std::make_unique<MemoryProvider>();
    high->values  = {{"conflict", std::string{"high"}}};
    auto low      = std::make_unique<MemoryProvider>();
    low->values   = {{"conflict", std::string{"low"}}};

    CompositeFeatureProvider composite(std::move(local));
    composite.addSource("high", std::move(high), 200);
    composite.addSource("low", std::move(low), 100);

    const auto loaded = composite.load();
    ASSERT_TRUE(loaded.ok);
    EXPECT_EQ(std::get<std::string>(loaded.value.at("conflict")), "high");
}

TEST(CompositeFeatureProviderTest, SynchronizedLocalSnapshotSupportsOfflineRestart)
{
    auto local        = std::make_unique<MemoryProvider>();
    auto *localAccess = local.get();
    local->values     = {{"conflict", std::string{"local"}}};

    auto remote    = std::make_unique<MemoryProvider>();
    remote->values = {{"conflict", std::string{"remote"}}};

    CompositeFeatureProvider online(std::move(local));
    online.addSource("remote", std::move(remote), 100);
    ASSERT_TRUE(online.load().ok);
    ASSERT_EQ(std::get<std::string>(localAccess->values.at("conflict")), "remote");

    auto offlineLocal       = std::make_unique<MemoryProvider>();
    offlineLocal->values    = localAccess->values;
    auto offlineRemote      = std::make_unique<MemoryProvider>();
    offlineRemote->failLoad = true;
    CompositeFeatureProvider offline(std::move(offlineLocal));
    offline.addSource("remote", std::move(offlineRemote), 100);

    const auto loaded = offline.load();
    ASSERT_TRUE(loaded.ok);
    EXPECT_EQ(std::get<std::string>(loaded.value.at("conflict")), "remote");
}

TEST(CompositeFeatureProviderTest, SuccessfulRemoteSnapshotCanDeletePreviouslyOwnedFeature)
{
    auto local         = std::make_unique<MemoryProvider>();
    local->values      = {{"localOnly", true}};
    auto remote        = std::make_unique<MemoryProvider>();
    auto *remoteAccess = remote.get();
    remote->values     = {{"remoteOwned", true}};

    CompositeFeatureProvider composite(std::move(local));
    composite.addSource("remote", std::move(remote), 100);
    ASSERT_TRUE(composite.load().ok);
    remoteAccess->values.clear();

    const auto loaded = composite.load();
    ASSERT_TRUE(loaded.ok);
    EXPECT_FALSE(loaded.value.contains("remoteOwned"));
    EXPECT_TRUE(loaded.value.contains("localOnly"));
}

TEST(CompositeFeatureProviderTest, SuccessfulRemoteDeletionSurvivesProviderRestart)
{
    auto local        = std::make_unique<MemoryProvider>();
    auto *localAccess = local.get();
    local->values     = {{"localOnly", true}};
    auto remote       = std::make_unique<MemoryProvider>();
    remote->values    = {{"remoteOwned", true}};

    CompositeFeatureProvider first(std::move(local));
    first.addSource("remote", std::move(remote), 100);
    EXPECT_TRUE(first.load().ok);

    auto restartedLocal    = std::make_unique<MemoryProvider>();
    restartedLocal->values = localAccess->values;
    auto emptyRemote       = std::make_unique<MemoryProvider>();
    CompositeFeatureProvider restarted(std::move(restartedLocal));
    restarted.addSource("remote", std::move(emptyRemote), 100);

    const auto loaded = restarted.load();
    EXPECT_TRUE(loaded.ok);
    EXPECT_FALSE(loaded.value.contains("remoteOwned"));
    EXPECT_TRUE(loaded.value.contains("localOnly"));
}

TEST(CompositeFeatureProviderTest, FactoryDoesNotWriteWhenNoRemoteSourceExists)
{
    auto local        = std::make_unique<MemoryProvider>();
    auto *localAccess = local.get();
    local->values     = {{"enabled", true}};
    auto provider     = makeCompositeFeatureProvider(std::move(local));

    const auto loaded = provider->load();
    EXPECT_TRUE(loaded.ok);
    EXPECT_EQ(localAccess->saveCalls, 0);
}

TEST(CompositeFeatureProviderTest, StableSourceIdsSurviveSourceReordering)
{
    auto local        = std::make_unique<MemoryProvider>();
    auto *localAccess = local.get();
    auto sourceA      = std::make_unique<MemoryProvider>();
    sourceA->values   = {{"conflict", std::string{"A"}}};
    auto sourceB      = std::make_unique<MemoryProvider>();
    sourceB->values   = {{"conflict", std::string{"B"}}};

    CompositeFeatureProvider first(std::move(local));
    first.addSource("source-a", std::move(sourceA), 100);
    first.addSource("source-b", std::move(sourceB), 200);
    ASSERT_TRUE(first.load().ok);

    auto restartedLocal    = std::make_unique<MemoryProvider>();
    restartedLocal->values = localAccess->values;
    auto offlineB          = std::make_unique<MemoryProvider>();
    offlineB->failLoad     = true;
    auto offlineA          = std::make_unique<MemoryProvider>();
    offlineA->failLoad     = true;
    CompositeFeatureProvider restarted(std::move(restartedLocal));
    restarted.addSource("source-b", std::move(offlineB), 200);
    restarted.addSource("source-a", std::move(offlineA), 100);

    const auto loaded = restarted.load();

    ASSERT_TRUE(loaded.ok);
    EXPECT_EQ(std::get<std::string>(loaded.value.at("conflict")), "B");
}

TEST(CompositeFeatureProviderTest, AggregatesSuccessfulChildProviderStatus)
{
    auto local             = std::make_unique<MemoryProvider>();
    local->providerStatus  = {true, {"directory durability uncertain"}};
    auto remote            = std::make_unique<MemoryProvider>();
    remote->providerStatus = {true, {"using cached response"}};
    CompositeFeatureProvider composite(std::move(local));
    composite.addSource("network", std::move(remote), 100);

    ASSERT_TRUE(composite.load().ok);
    const auto status = composite.status();

    EXPECT_TRUE(status.stale);
    ASSERT_EQ(status.warnings.size(), 2);
    EXPECT_NE(status.warnings[0].find("local feature cache"), std::string::npos);
    EXPECT_NE(status.warnings[1].find("network"), std::string::npos);
    EXPECT_NE(status.warnings[1].find("using cached response"), std::string::npos);
}

TEST(CompositeFeatureProviderTest, RejectsDuplicateSourceIds)
{
    auto local = std::make_unique<MemoryProvider>();
    CompositeFeatureProvider composite(std::move(local));
    composite.addSource("remote", std::make_unique<MemoryProvider>(), 100);

    EXPECT_THROW(composite.addSource("remote", std::make_unique<MemoryProvider>(), 200), std::invalid_argument);
}

TEST(CompositeFeatureProviderTest, RejectsReservedMetadataFeatureNames)
{
    auto local  = std::make_unique<MemoryProvider>();
    auto remote = std::make_unique<MemoryProvider>();
    CompositeFeatureProvider composite(std::move(local));
    composite.addSource("remote", std::move(remote), 100);
    ASSERT_TRUE(composite.load().ok);

    const auto saved = composite.save({{"__szx_feature_manager_v1.user", true}});

    EXPECT_FALSE(saved.ok);
}

} // namespace
