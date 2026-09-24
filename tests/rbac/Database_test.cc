#include "SzxAdmin/Infra/Database.h"
#include "../../tools/szx_admin/src/infra/DatabaseUnitOfWork.h"
#include "../../tools/szx_admin/src/rbac/PermissionCodes.h"
#include "../../tools/szx_admin/src/maintenance/LogMaintenanceService.h"
#include "../../tools/szx_admin/src/telemetry/BehaviorEventWriter.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Random.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using SZX::ADMIN::INFRA::AppliedMigration;
using SZX::ADMIN::INFRA::BootstrapAdministrator;
using SZX::ADMIN::INFRA::BootstrapAdminService;
using SZX::ADMIN::INFRA::DatabaseConfig;
using SZX::ADMIN::INFRA::DatabaseConnection;
using SZX::ADMIN::INFRA::ErrorCode;
using SZX::ADMIN::INFRA::MigrationDefinition;
using SZX::ADMIN::INFRA::MigrationRunner;

namespace
{
using SZX::ADMIN::MAINTENANCE::LogMaintenanceRepository;
using SZX::ADMIN::MAINTENANCE::LogPurgeBatch;
using SZX::ADMIN::TELEMETRY::BatchWriteOutcome;
using SZX::ADMIN::TELEMETRY::BehaviorEventFilter;
using SZX::ADMIN::TELEMETRY::BehaviorEventPage;
using SZX::ADMIN::TELEMETRY::BehaviorEventRecord;
using SZX::ADMIN::TELEMETRY::BehaviorEventRepository;

auto migrationChecksum(std::string_view sql) -> std::string
{
    const auto digest =
        SZX::CRYPTO::sha256KeyId(std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(sql.data()), sql.size()});
    if (!digest.ok)
        throw std::runtime_error("Cannot calculate test migration checksum");
    return digest.value;
}

class FakeBehaviorEventRepository final : public BehaviorEventRepository
{
public:
    auto appendBatch(std::span<const BehaviorEventRecord> records) -> SZX::COMMON::Result<BatchWriteOutcome> override
    {
        std::lock_guard lock(mutex);
        batches.emplace_back(records.begin(), records.end());
        const auto outcome = outcomes.empty() ? fallback : outcomes.front();
        if (!outcomes.empty())
            outcomes.pop_front();
        changed.notify_all();
        return SZX::COMMON::Result<BatchWriteOutcome>::Success(outcome);
    }

    auto probe() -> SZX::COMMON::Result<void> override
    {
        ++probeCount;
        return failProbe.load() ? SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {}) :
                                  SZX::COMMON::Result<void>::Success();
    }

    auto query(const BehaviorEventFilter &) -> SZX::COMMON::Result<BehaviorEventPage> override
    {
        return SZX::COMMON::Result<BehaviorEventPage>::Success({});
    }

    auto waitForBatches(std::size_t count, std::chrono::milliseconds timeout) -> bool
    {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, timeout, [&] { return batches.size() >= count; });
    }

    std::mutex mutex;
    std::condition_variable changed;
    std::deque<BatchWriteOutcome> outcomes;
    std::vector<std::vector<BehaviorEventRecord>> batches;
    BatchWriteOutcome fallback{BatchWriteOutcome::ReadyToCommit};
    std::atomic_bool failProbe{};
    std::atomic_int probeCount{};
};

class FakeLogMaintenanceRepository final : public LogMaintenanceRepository
{
public:
    auto purgeAuditBatch(std::string cutoff, std::uint32_t batchSize) -> SZX::COMMON::Result<LogPurgeBatch> override
    {
        auditCutoffs.push_back(std::move(cutoff));
        requestedBatchSizes.push_back(batchSize);
        const auto batch = auditPurgeBatches.empty() ? LogPurgeBatch{} : auditPurgeBatches.front();
        if (!auditPurgeBatches.empty())
            auditPurgeBatches.pop_front();
        return SZX::COMMON::Result<LogPurgeBatch>::Success(batch);
    }

    auto purgeBehaviorBatch(std::string cutoff, std::uint32_t batchSize) -> SZX::COMMON::Result<LogPurgeBatch> override
    {
        behaviorCutoffs.push_back(std::move(cutoff));
        requestedBatchSizes.push_back(batchSize);
        const auto batch = behaviorPurgeBatches.empty() ? LogPurgeBatch{} : behaviorPurgeBatches.front();
        if (!behaviorPurgeBatches.empty())
            behaviorPurgeBatches.pop_front();
        return SZX::COMMON::Result<LogPurgeBatch>::Success(batch);
    }

    std::deque<LogPurgeBatch> auditPurgeBatches;
    std::deque<LogPurgeBatch> behaviorPurgeBatches;
    std::vector<std::uint32_t> requestedBatchSizes;
    std::vector<std::string> auditCutoffs;
    std::vector<std::string> behaviorCutoffs;
};

class FakeAuditRepository final : public SZX::ADMIN::AUDIT::AuditRepository
{
public:
    auto append(const SZX::ADMIN::AUDIT::AuditRecord &record) -> SZX::COMMON::Result<void> override
    {
        records.push_back(record);
        return failAppend ? SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::AuditWriteFailed), {}) :
                            SZX::COMMON::Result<void>::Success();
    }

    auto query(const SZX::ADMIN::AUDIT::AuditFilter &) -> SZX::COMMON::Result<SZX::ADMIN::AUDIT::AuditPage> override
    {
        return SZX::COMMON::Result<SZX::ADMIN::AUDIT::AuditPage>::Success({});
    }

    std::vector<SZX::ADMIN::AUDIT::AuditRecord> records;
    bool failAppend{};
};

class FakeDatabaseConnection final : public DatabaseConnection
{
public:
    auto behaviorEventRepository() noexcept -> BehaviorEventRepository * override
    {
        return behaviorRepository;
    }

    auto logMaintenanceRepository() noexcept -> LogMaintenanceRepository * override
    {
        return maintenanceRepository;
    }

    auto auditRepository() noexcept -> SZX::ADMIN::AUDIT::AuditRepository * override
    {
        return auditRepositoryValue;
    }

    auto executeMigrationStatement(std::string_view sql) -> SZX::COMMON::Result<void> override
    {
        statements.emplace_back(sql);
        if (!failOn.empty() && sql.find(failOn) != std::string_view::npos)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), "query failed");
        return SZX::COMMON::Result<void>::Success();
    }

    auto loadAppliedMigrations() -> SZX::COMMON::Result<std::vector<AppliedMigration>> override
    {
        return SZX::COMMON::Result<std::vector<AppliedMigration>>::Success(applied);
    }

    auto recordMigration(const AppliedMigration &migration) -> SZX::COMMON::Result<void> override
    {
        if (throwOnJournal)
            throw std::runtime_error("injected journal failure");
        recorded.push_back(migration);
        applied.push_back(migration);
        return SZX::COMMON::Result<void>::Success();
    }

    auto acquireMigrationLock(std::string_view, std::uint32_t) -> SZX::COMMON::Result<bool> override
    {
        ++acquireLockCount;
        events.emplace_back("lock");
        return SZX::COMMON::Result<bool>::Success(lockAcquired);
    }

    auto releaseMigrationLock(std::string_view) -> SZX::COMMON::Result<void> override
    {
        ++releaseLockCount;
        events.emplace_back("unlock");
        return SZX::COMMON::Result<void>::Success();
    }

    auto begin() -> SZX::COMMON::Result<void> override
    {
        ++beginCount;
        events.emplace_back("begin");
        savedAdmin  = insertedAdmin;
        savedDone   = bootstrapDone;
        savedUserId = bootstrapUserId;
        return SZX::COMMON::Result<void>::Success();
    }

    auto commit() -> SZX::COMMON::Result<void> override
    {
        ++commitCount;
        events.emplace_back("commit");
        if (failCommit)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseTransactionFailed), "commit failed");
        return SZX::COMMON::Result<void>::Success();
    }

    auto commitWithOutcome() -> SZX::COMMON::Result<SZX::ADMIN::INFRA::CommitOutcome> override
    {
        if (commitOutcomes.empty())
            return DatabaseConnection::commitWithOutcome();
        ++commitCount;
        events.emplace_back("commit");
        const auto outcome = commitOutcomes.front();
        commitOutcomes.pop_front();
        return SZX::COMMON::Result<SZX::ADMIN::INFRA::CommitOutcome>::Success(outcome);
    }

    auto rollback() -> SZX::COMMON::Result<void> override
    {
        ++rollbackCount;
        events.emplace_back("rollback");
        insertedAdmin   = savedAdmin;
        bootstrapDone   = savedDone;
        bootstrapUserId = savedUserId;
        return SZX::COMMON::Result<void>::Success();
    }

    void invalidate() noexcept override
    {
        ++invalidateCount;
        reusableState = false;
    }

    auto reusable() const noexcept -> bool override
    {
        return reusableState;
    }

    auto bootstrapCompleted() -> SZX::COMMON::Result<bool> override
    {
        return SZX::COMMON::Result<bool>::Success(bootstrapDone);
    }

    auto findBootstrapAdministrator() -> SZX::COMMON::Result<std::optional<BootstrapAdministrator>> override
    {
        return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Success(existingAdmin);
    }

    auto insertBootstrapAdministrator(const BootstrapAdministrator &administrator) -> SZX::COMMON::Result<void> override
    {
        insertedAdmin = administrator;
        return SZX::COMMON::Result<void>::Success();
    }

    auto recordBootstrapCompletion(std::string_view userId) -> SZX::COMMON::Result<void> override
    {
        if (throwOnMark)
            throw std::runtime_error("injected marker failure");
        if (failMark)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), "marker failed");
        bootstrapDone   = true;
        bootstrapUserId = std::string{userId};
        return SZX::COMMON::Result<void>::Success();
    }

    std::vector<std::string> statements;
    std::vector<AppliedMigration> applied;
    std::vector<AppliedMigration> recorded;
    std::string failOn;
    int beginCount{};
    int commitCount{};
    int rollbackCount{};
    int acquireLockCount{};
    int releaseLockCount{};
    bool lockAcquired{true};
    bool bootstrapDone{};
    std::optional<BootstrapAdministrator> existingAdmin;
    std::optional<BootstrapAdministrator> insertedAdmin;
    std::string bootstrapUserId;
    std::vector<std::string> events;
    std::optional<BootstrapAdministrator> savedAdmin;
    bool savedDone{};
    std::string savedUserId;
    bool failCommit{};
    bool failMark{};
    bool throwOnMark{};
    bool throwOnJournal{};
    BehaviorEventRepository *behaviorRepository{};
    LogMaintenanceRepository *maintenanceRepository{};
    SZX::ADMIN::AUDIT::AuditRepository *auditRepositoryValue{};
    std::deque<SZX::ADMIN::INFRA::CommitOutcome> commitOutcomes;
    int invalidateCount{};
    bool reusableState{true};
};

auto event(std::string id) -> BehaviorEventRecord
{
    return {.id           = std::move(id),
            .user_id      = "10000000-0000-4000-8000-000000000001",
            .event_name   = "report.view",
            .result       = "success",
            .request_id   = "request-1",
            .ip           = "127.0.0.1",
            .user_agent   = "test",
            .details_json = "{}",
            .created_at   = "2026-09-21 00:00:00.000000"};
}

auto writerConfig() -> SZX::ADMIN::TELEMETRY::TelemetryConfig
{
    return {.queueCapacity              = 8,
            .batchSize                  = 2,
            .flushIntervalMs            = 10,
            .minCommitIntervalMs        = 1,
            .retryInitialMs             = 1,
            .retryMaxMs                 = 4,
            .databaseOperationTimeoutMs = 50,
            .shutdownFlushTimeoutMs     = 250,
            .retryAfterSeconds          = 1};
}

} // namespace

TEST(DatabaseConfigTest, RequiresDatabaseIdentity)
{
    const auto result = DatabaseConfig{}.validate();

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::InvalidConfiguration));
}

TEST(DatabaseConfigTest, AcceptsValidConnectionSettings)
{
    DatabaseConfig config;
    config.database = "szx_rbac";
    config.username = "szx_admin";

    const auto result = config.validate();

    EXPECT_TRUE(result.ok) << result.message;
}

TEST(DatabaseConfigTest, RejectsUnsafeDatabaseIdentifier)
{
    DatabaseConfig config;
    config.database = "szx-rbac";
    config.username = "szx_admin";

    const auto result = config.validate();

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::InvalidConfiguration));
}

TEST(TelemetryConfigTest, AcceptsBurstQueueWithoutMinuteRateLimit)
{
    SZX::ADMIN::TELEMETRY::TelemetryConfig config;
    config.queueCapacity       = 10000;
    config.batchSize           = 200;
    config.flushIntervalMs     = 200;
    config.minCommitIntervalMs = 200;

    const auto result = config.validate();

    ASSERT_TRUE(result.ok) << result.message;
}

TEST(TelemetryConfigTest, RejectsInvalidBatchAndRetryBounds)
{
    SZX::ADMIN::TELEMETRY::TelemetryConfig config;
    config.queueCapacity  = 10;
    config.batchSize      = 11;
    config.retryInitialMs = 1000;
    config.retryMaxMs     = 500;

    const auto result = config.validate();

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::InvalidConfiguration));
}

TEST(BehaviorEventWriterTest, DrainsBurstInConfiguredBatches)
{
    FakeBehaviorEventRepository repository;
    FakeDatabaseConnection connection;
    connection.behaviorRepository = &repository;
    SZX::ADMIN::TELEMETRY::BehaviorEventWriter writer([&](const auto &callback, auto) { return callback(connection); },
                                                      writerConfig());

    ASSERT_TRUE(writer.start().ok);
    ASSERT_TRUE(writer.enqueue(event("event-1")).ok);
    ASSERT_TRUE(writer.enqueue(event("event-2")).ok);
    ASSERT_TRUE(writer.enqueue(event("event-3")).ok);
    const auto drained = writer.stop();

    EXPECT_TRUE(drained.clean);
    std::lock_guard lock(repository.mutex);
    ASSERT_EQ(repository.batches.size(), 2U);
    EXPECT_EQ(repository.batches[0].size(), 2U);
    EXPECT_EQ(repository.batches[1].size(), 1U);
    EXPECT_EQ(connection.commitCount, 2);
}

TEST(BehaviorEventWriterTest, ResolvesUnknownCommitWithSameEventId)
{
    FakeBehaviorEventRepository repository;
    repository.outcomes = {BatchWriteOutcome::ReadyToCommit, BatchWriteOutcome::AlreadyCommitted};
    FakeDatabaseConnection connection;
    connection.behaviorRepository = &repository;
    connection.commitOutcomes.push_back(SZX::ADMIN::INFRA::CommitOutcome::Unknown);
    SZX::ADMIN::TELEMETRY::BehaviorEventWriter writer([&](const auto &callback, auto) { return callback(connection); },
                                                      writerConfig());

    ASSERT_TRUE(writer.start().ok);
    ASSERT_TRUE(writer.enqueue(event("stable-event-id")).ok);
    ASSERT_TRUE(repository.waitForBatches(2, std::chrono::seconds{1}));
    const auto drained = writer.stop();

    EXPECT_TRUE(drained.clean);
    EXPECT_EQ(drained.pendingCommitUnknown, 0U);
    EXPECT_EQ(connection.invalidateCount, 1);
    std::lock_guard lock(repository.mutex);
    ASSERT_EQ(repository.batches.size(), 2U);
    EXPECT_EQ(repository.batches[0][0].id, repository.batches[1][0].id);
}

TEST(BehaviorEventWriterTest, RequiresSuccessfulProbeBeforeResumingFaultedWriter)
{
    FakeBehaviorEventRepository repository;
    repository.outcomes = {BatchWriteOutcome::SystemicPermanentFailure, BatchWriteOutcome::ReadyToCommit};
    FakeDatabaseConnection connection;
    connection.behaviorRepository = &repository;
    SZX::ADMIN::TELEMETRY::BehaviorEventWriter writer([&](const auto &callback, auto) { return callback(connection); },
                                                      writerConfig());

    ASSERT_TRUE(writer.start().ok);
    ASSERT_TRUE(writer.enqueue(event("faulted-event")).ok);
    ASSERT_TRUE(repository.waitForBatches(1, std::chrono::seconds{1}));
    for (int attempt = 0; attempt < 100 && writer.state() != SZX::ADMIN::TELEMETRY::AdmissionState::Faulted; ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    ASSERT_EQ(writer.state(), SZX::ADMIN::TELEMETRY::AdmissionState::Faulted);

    repository.failProbe.store(true);
    EXPECT_FALSE(writer.resumeAfterFault().ok);
    EXPECT_EQ(writer.state(), SZX::ADMIN::TELEMETRY::AdmissionState::Faulted);
    repository.failProbe.store(false);
    EXPECT_TRUE(writer.resumeAfterFault().ok);
    ASSERT_TRUE(repository.waitForBatches(2, std::chrono::seconds{1}));
    EXPECT_TRUE(writer.stop().clean);
    EXPECT_EQ(repository.probeCount.load(), 2);
}

TEST(BehaviorEventWriterTest, ShutdownDeadlineReportsDiscardedRetryingBatch)
{
    FakeBehaviorEventRepository repository;
    repository.fallback = BatchWriteOutcome::RetryableFailure;
    FakeDatabaseConnection connection;
    connection.behaviorRepository     = &repository;
    auto config                       = writerConfig();
    config.databaseOperationTimeoutMs = 10;
    config.shutdownFlushTimeoutMs     = 20;
    SZX::ADMIN::TELEMETRY::BehaviorEventWriter writer([&](const auto &callback, auto) { return callback(connection); }, config);

    ASSERT_TRUE(writer.start().ok);
    ASSERT_TRUE(writer.enqueue(event("retrying-event")).ok);
    ASSERT_TRUE(repository.waitForBatches(1, std::chrono::seconds{1}));
    const auto drained = writer.stop();

    EXPECT_FALSE(drained.clean);
    EXPECT_EQ(drained.discarded, 1U);
}

TEST(LogMaintenanceServiceTest, PurgeDeletesCurrentTablesWithAuditInEachTransaction)
{
    FakeLogMaintenanceRepository repository;
    repository.behaviorPurgeBatches = {{.deleted = 2}, {}};
    repository.auditPurgeBatches    = {{.deleted = 3}, {}};
    FakeAuditRepository audit;
    FakeDatabaseConnection connection;
    connection.maintenanceRepository = &repository;
    connection.auditRepositoryValue  = &audit;
    SZX::ADMIN::INFRA::LogStorageConfig config;
    config.cleanupBatchSize = 23;
    SZX::ADMIN::MAINTENANCE::LogMaintenanceService service([&](const auto &callback) { return callback(connection); }, config);

    const auto result = service.purge();

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.value.behaviorDeleted, 2U);
    EXPECT_EQ(result.value.auditDeleted, 3U);
    ASSERT_EQ(audit.records.size(), 2U);
    EXPECT_EQ(audit.records[0].event_name, "user.behavior_events.purged");
    EXPECT_EQ(audit.records[1].event_name, "user.audit_logs.purged");
    EXPECT_EQ(connection.commitCount, 2);
    EXPECT_EQ(connection.rollbackCount, 2);
    EXPECT_EQ(connection.releaseLockCount, 1);
    EXPECT_EQ(repository.requestedBatchSizes, (std::vector<std::uint32_t>{23, 23, 23, 23}));
}

TEST(LogMaintenanceServiceTest, PurgeRollsBackDeletionWhenAuditWriteFails)
{
    FakeLogMaintenanceRepository repository;
    repository.behaviorPurgeBatches = {{.deleted = 1}};
    FakeAuditRepository audit;
    audit.failAppend = true;
    FakeDatabaseConnection connection;
    connection.maintenanceRepository = &repository;
    connection.auditRepositoryValue  = &audit;
    SZX::ADMIN::MAINTENANCE::LogMaintenanceService service([&](const auto &callback) { return callback(connection); }, {});

    const auto result = service.purge();

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::AuditWriteFailed));
    EXPECT_EQ(connection.commitCount, 0);
    EXPECT_EQ(connection.rollbackCount, 1);
    EXPECT_EQ(connection.releaseLockCount, 1);
}

TEST(DatabaseInitializationTest, PreservesPublishedMigrationChecksumsAndExistingJournal)
{
    // Fingerprints from the previously released SQL files: moving SQL must not
    // change its identity or cause already-initialized databases to be rejected.
    FakeDatabaseConnection connection;
    connection.applied    = {{1, "86a4ac820cc8457a2131d0400d4968268576b17113f38fe86b6beceea48c2047"},
                             {2, "f4223aadebf005f145941d06b89d2d41e964f4c059d2f765a8e1b3678d389d2d"}};
    const auto migrations = SZX::ADMIN::INFRA::embeddedMigrations();
    ASSERT_GE(migrations.size(), connection.applied.size());
    for (std::size_t index = 0; index < connection.applied.size(); ++index)
    {
        const auto &migration = migrations[index];
        const auto digest     = SZX::CRYPTO::sha256KeyId(
            std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(migration.sql.data()), migration.sql.size()});
        ASSERT_TRUE(digest.ok);
        EXPECT_EQ(migration.version, connection.applied[index].version);
        EXPECT_EQ(digest.value, connection.applied[index].checksum);
        EXPECT_EQ(migration.checksum, digest.value);
    }
    const auto repeated = MigrationRunner{}.run(connection, migrations.first(connection.applied.size()));
    ASSERT_TRUE(repeated.ok) << repeated.message;
    EXPECT_EQ(repeated.value.appliedCount, 0U);
    EXPECT_TRUE(connection.recorded.empty());
    EXPECT_EQ(connection.statements.size(), 1U); // Only ensures the journal table exists.
}

TEST(DatabaseInitializationTest, DropsLegacyLogTableWithoutRewritingPublishedMigrations)
{
    const auto migrations = SZX::ADMIN::INFRA::embeddedMigrations();
    ASSERT_EQ(migrations.size(), 6U);
    FakeDatabaseConnection connection;
    const auto legacy = migrations.first(5);
    for (const auto &migration : legacy.first(4))
        connection.applied.push_back({migration.version, std::string{migration.checksum}});
    const auto upgrade = MigrationRunner{}.run(connection, legacy);
    ASSERT_TRUE(upgrade.ok);
    EXPECT_EQ(upgrade.value.currentVersion, 5U);
    EXPECT_EQ(upgrade.value.appliedCount, 1U);
    ASSERT_EQ(connection.recorded.size(), 1U);
    EXPECT_EQ(connection.recorded.front().checksum, migrationChecksum(migrations[4].sql));
    EXPECT_EQ(connection.recorded.front().checksum, "df5a342ab4da5db2515c8469d3fe17751a6570cc6ee9b009bef9bed9d2463241");
    EXPECT_TRUE(std::ranges::any_of(connection.statements, [](const auto &sql)
                                    { return sql.find("DROP TABLE IF EXISTS user_logs") != std::string::npos; }));
    const auto repeated = MigrationRunner{}.run(connection, legacy);
    ASSERT_TRUE(repeated.ok);
    EXPECT_EQ(repeated.value.appliedCount, 0U);
}

TEST(DatabaseInitializationTest, RegistersTheDynamicRbacCatalog)
{
    const auto migrations = SZX::ADMIN::INFRA::embeddedMigrations();
    ASSERT_EQ(migrations.back().version, 6U);
    EXPECT_NE(migrations.back().sql.find("CREATE TABLE IF NOT EXISTS roles"), std::string_view::npos);
    EXPECT_NE(migrations.back().sql.find("CREATE TABLE IF NOT EXISTS permissions"), std::string_view::npos);
    EXPECT_NE(migrations.back().sql.find("role_manage_scopes"), std::string_view::npos);
    EXPECT_EQ(SZX::ADMIN::RBAC::PERMISSIONS::definitions().size(), 24U);
}

TEST(MigrationRunnerTest, AppliesPendingMigrationAndRecordsIt)
{
    const std::string sql = "CREATE TABLE test_table (name VARCHAR(32)); INSERT INTO test_table(name) VALUES ('semi;colon');";
    const auto checksum   = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};

    FakeDatabaseConnection connection;
    const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.value.currentVersion, 1U);
    EXPECT_EQ(result.value.appliedCount, 1U);
    EXPECT_EQ(connection.beginCount, 1);
    EXPECT_EQ(connection.commitCount, 1);
    EXPECT_EQ(connection.rollbackCount, 0);
    EXPECT_EQ(connection.acquireLockCount, 1);
    EXPECT_EQ(connection.releaseLockCount, 1);
    ASSERT_EQ(connection.recorded.size(), 1U);
    ASSERT_EQ(connection.statements.size(), 3U);
    EXPECT_NE(connection.statements[0].find("schema_migrations"), std::string::npos);
    EXPECT_NE(connection.statements[2].find("semi;colon"), std::string::npos);
}

TEST(MigrationRunnerTest, RejectsChecksumConflict)
{
    const std::string sql = "CREATE TABLE test_table (id INT);";
    const auto checksum   = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};

    FakeDatabaseConnection connection;
    connection.applied.push_back(AppliedMigration{1, "0000000000000000"});
    const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationConflict));
    EXPECT_EQ(connection.beginCount, 0);
}

TEST(MigrationRunnerTest, StopsWhenAnotherInstanceOwnsMigrationLock)
{
    const std::string sql = "CREATE TABLE test_table (id INT);";
    const auto checksum   = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};

    FakeDatabaseConnection connection;
    connection.lockAcquired = false;
    const auto result       = MigrationRunner{}.run(connection, std::span{&migration, 1});

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationFailed));
    EXPECT_EQ(connection.statements.size(), 0U);
    EXPECT_EQ(connection.releaseLockCount, 0);
}

TEST(MigrationRunnerTest, DoesNotStartTransactionWhenDdlFails)
{
    const std::string sql = "CREATE TABLE test_table (id INT); FAIL THIS STATEMENT;";
    const auto checksum   = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};

    FakeDatabaseConnection connection;
    connection.failOn = "FAIL THIS STATEMENT";
    const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationFailed));
    EXPECT_EQ(connection.beginCount, 0);
    EXPECT_EQ(connection.commitCount, 0);
    EXPECT_EQ(connection.rollbackCount, 0);
}

TEST(BootstrapAdminTest, ExistingStateIsIdempotent)
{
    FakeDatabaseConnection connection;
    connection.bootstrapDone = true;

    const auto result = BootstrapAdminService{}.run(connection);

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_FALSE(result.value);
    EXPECT_FALSE(connection.insertedAdmin.has_value());
}

TEST(BootstrapAdminTest, ExistingSuperAdminIsMarkedWithoutReset)
{
    FakeDatabaseConnection connection;
    connection.existingAdmin = BootstrapAdministrator{"existing-id", "renamed", "hash", "super_admin", "active", true};

    const auto result = BootstrapAdminService{}.run(connection);

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_FALSE(result.value);
    EXPECT_FALSE(connection.insertedAdmin.has_value());
    EXPECT_EQ(connection.bootstrapUserId, "existing-id");
}

TEST(BootstrapAdminTest, RejectsNonSuperAdminBootstrapUsername)
{
    FakeDatabaseConnection connection;
    connection.existingAdmin = BootstrapAdministrator{"existing-id", "admin", "hash", "operator", "active", false};

    const auto result = BootstrapAdminService{}.run(connection);

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::UserIdentityConflict));
}

TEST(BootstrapAdminTest, CreatesOnceAndCommitsBeforeUnlock)
{
    FakeDatabaseConnection connection;
    const auto created = BootstrapAdminService{}.run(connection);
    ASSERT_TRUE(created.ok) << created.message;
    EXPECT_TRUE(created.value);
    ASSERT_TRUE(connection.insertedAdmin.has_value());
    const auto administrator = *connection.insertedAdmin;
    EXPECT_EQ(administrator.username, "admin");
    EXPECT_EQ(administrator.role, "super_admin");
    EXPECT_TRUE(administrator.forcePasswordChange);
    EXPECT_FALSE(administrator.passwordHash.empty());
    EXPECT_TRUE(administrator.passwordHash.starts_with("sha256-v1:$argon2id$"));
    EXPECT_NE(administrator.passwordHash, "superadmin");
    EXPECT_EQ(connection.bootstrapUserId, administrator.id);
    EXPECT_EQ(connection.events, (std::vector<std::string>{"lock", "begin", "commit", "unlock"}));

    const auto repeated = BootstrapAdminService{}.run(connection);
    ASSERT_TRUE(repeated.ok);
    EXPECT_FALSE(repeated.value);
    EXPECT_EQ(connection.insertedAdmin->id, administrator.id);
    EXPECT_EQ(connection.insertedAdmin->passwordHash, administrator.passwordHash);
}

TEST(BootstrapAdminTest, FailuresRollBackBeforeUnlockAndAllowRetry)
{
    for (const auto failure : {"marker", "exception", "commit"})
    {
        SCOPED_TRACE(failure);
        FakeDatabaseConnection connection;
        connection.failMark    = std::string_view{failure} == "marker";
        connection.throwOnMark = std::string_view{failure} == "exception";
        connection.failCommit  = std::string_view{failure} == "commit";
        const auto failed      = BootstrapAdminService{}.run(connection);
        EXPECT_FALSE(failed.ok);
        EXPECT_FALSE(connection.insertedAdmin.has_value());
        EXPECT_FALSE(connection.bootstrapDone);
        ASSERT_GE(connection.events.size(), 4U);
        EXPECT_EQ(connection.events[connection.events.size() - 2], "rollback");
        EXPECT_EQ(connection.events.back(), "unlock");
        connection.failMark = connection.throwOnMark = connection.failCommit = false;
        const auto retried                                                   = BootstrapAdminService{}.run(connection);
        ASSERT_TRUE(retried.ok) << retried.message;
        EXPECT_TRUE(retried.value);
    }
}

TEST(MigrationRunnerTest, RepeatedRunSkipsAppliedDdl)
{
    FakeDatabaseConnection connection;
    const std::string sql = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const auto checksum   = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};
    ASSERT_TRUE(MigrationRunner{}.run(connection, std::span{&migration, 1}).ok);
    connection.statements.clear();
    const auto repeated = MigrationRunner{}.run(connection, std::span{&migration, 1});
    ASSERT_TRUE(repeated.ok);
    EXPECT_EQ(repeated.value.appliedCount, 0U);
    EXPECT_EQ(connection.statements.size(), 1U); // Only the journal's idempotent CREATE.
    EXPECT_EQ(connection.recorded.size(), 1U);
}

TEST(MigrationRunnerTest, JournalExceptionRollsBackBeforeUnlock)
{
    FakeDatabaseConnection connection;
    connection.throwOnJournal = true;
    const std::string sql     = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const auto checksum       = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};
    const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationFailed));
    EXPECT_EQ(connection.events, (std::vector<std::string>{"lock", "begin", "rollback", "unlock"}));
}

TEST(MigrationRunnerTest, RejectsChangedSqlWithStaleChecksumBeforeDatabaseAccess)
{
    const std::string originalSql = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const std::string changedSql  = "CREATE TABLE IF NOT EXISTS test_table (id BIGINT);";
    const auto staleChecksum      = migrationChecksum(originalSql);
    const MigrationDefinition migration{1, changedSql, staleChecksum};
    for (const auto alreadyApplied : {false, true})
    {
        SCOPED_TRACE(alreadyApplied);
        FakeDatabaseConnection connection;
        if (alreadyApplied)
            connection.applied = {{1, staleChecksum}};
        const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});

        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationConflict));
        EXPECT_EQ(connection.acquireLockCount, 0);
        EXPECT_TRUE(connection.statements.empty());
    }
}

TEST(MigrationRunnerTest, RejectsDatabaseMigrationNewerThanProgram)
{
    const std::string sql = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const auto checksum   = migrationChecksum(sql);
    const MigrationDefinition migration{1, sql, checksum};
    FakeDatabaseConnection connection;
    connection.applied = {{1, checksum}, {2, std::string(64, 'a')}};

    const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationConflict));
    EXPECT_EQ(connection.beginCount, 0);
    EXPECT_TRUE(connection.recorded.empty());
}

TEST(MigrationRunnerTest, RejectsUnknownJournalVersionInsideSupportedRange)
{
    const std::string sql                   = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const auto checksum                     = migrationChecksum(sql);
    const MigrationDefinition definitions[] = {{1, sql, checksum}, {3, sql, checksum}};
    FakeDatabaseConnection connection;
    connection.applied = {{2, checksum}};

    const auto result = MigrationRunner{}.run(connection, definitions);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationConflict));
    EXPECT_EQ(connection.statements.size(), 1U);
    EXPECT_TRUE(connection.recorded.empty());
}

TEST(MigrationRunnerTest, ValidatesEntireCatalogBeforeApplyingPendingMigrations)
{
    const std::string sql                   = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const auto checksum                     = migrationChecksum(sql);
    const MigrationDefinition definitions[] = {{1, sql, checksum}, {2, sql, std::string_view{"invalid"}}};
    FakeDatabaseConnection connection;

    const auto result = MigrationRunner{}.run(connection, definitions);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationConflict));
    EXPECT_TRUE(connection.statements.empty());
    EXPECT_TRUE(connection.recorded.empty());
}

TEST(MigrationRunnerTest, ValidatesEntireJournalBeforeApplyingPendingMigrations)
{
    const std::string sql                   = "CREATE TABLE IF NOT EXISTS test_table (id INT);";
    const auto checksum                     = migrationChecksum(sql);
    const MigrationDefinition definitions[] = {{1, sql, checksum}, {2, sql, checksum}};
    FakeDatabaseConnection connection;
    connection.applied = {{2, std::string(64, 'a')}};

    const auto result = MigrationRunner{}.run(connection, definitions);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationConflict));
    EXPECT_EQ(connection.statements.size(), 1U);
    EXPECT_TRUE(connection.recorded.empty());
}

TEST(DatabaseUnitOfWorkTest, RollsBackAnUncommittedTransactionAtScopeExit)
{
    FakeDatabaseConnection connection;
    {
        SZX::ADMIN::INFRA::DatabaseUnitOfWork unit(connection);
        ASSERT_TRUE(unit.begin().ok);
        EXPECT_TRUE(unit.inTransaction());
    }
    EXPECT_EQ(connection.beginCount, 1);
    EXPECT_EQ(connection.commitCount, 0);
    EXPECT_EQ(connection.rollbackCount, 1);
}

TEST(DatabaseUnitOfWorkTest, DoesNotRollbackAfterSuccessfulCommit)
{
    FakeDatabaseConnection connection;
    {
        SZX::ADMIN::INFRA::DatabaseUnitOfWork unit(connection);
        ASSERT_TRUE(unit.begin().ok);
        ASSERT_TRUE(unit.commit().ok);
        EXPECT_FALSE(unit.inTransaction());
    }
    EXPECT_EQ(connection.commitCount, 1);
    EXPECT_EQ(connection.rollbackCount, 0);
}
