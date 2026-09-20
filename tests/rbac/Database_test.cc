#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Random.h"

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
auto migrationChecksum(std::string_view sql) -> std::string
{
    const auto digest =
        SZX::CRYPTO::sha256KeyId(std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(sql.data()), sql.size()});
    if (!digest.ok)
        throw std::runtime_error("Cannot calculate test migration checksum");
    return digest.value;
}

class FakeDatabaseConnection final : public DatabaseConnection
{
public:
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

    auto rollback() -> SZX::COMMON::Result<void> override
    {
        ++rollbackCount;
        events.emplace_back("rollback");
        insertedAdmin   = savedAdmin;
        bootstrapDone   = savedDone;
        bootstrapUserId = savedUserId;
        return SZX::COMMON::Result<void>::Success();
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
};

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

TEST(DatabaseInitializationTest, UpgradesCanonicalIndexesWithoutRewritingPublishedMigrations)
{
    const auto migrations = SZX::ADMIN::INFRA::embeddedMigrations();
    ASSERT_EQ(migrations.size(), 3U);
    FakeDatabaseConnection connection;
    for (const auto &migration : migrations.first(2))
        connection.applied.push_back({migration.version, std::string{migration.checksum}});
    const auto upgrade = MigrationRunner{}.run(connection);
    ASSERT_TRUE(upgrade.ok);
    EXPECT_EQ(upgrade.value.currentVersion, 3U);
    EXPECT_EQ(upgrade.value.appliedCount, 1U);
    ASSERT_EQ(connection.recorded.size(), 1U);
    EXPECT_EQ(connection.recorded.front().checksum, migrationChecksum(migrations[2].sql));
    EXPECT_NE(connection.statements.back().find("utf8mb4_0900_bin"), std::string::npos);
    const auto repeated = MigrationRunner{}.run(connection);
    ASSERT_TRUE(repeated.ok);
    EXPECT_EQ(repeated.value.appliedCount, 0U);
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
