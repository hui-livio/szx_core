#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using SZX::ADMIN::INFRA::AppliedMigration;
using SZX::ADMIN::INFRA::DatabaseConfig;
using SZX::ADMIN::INFRA::DatabaseConnection;
using SZX::ADMIN::INFRA::ErrorCode;
using SZX::ADMIN::INFRA::MigrationDefinition;
using SZX::ADMIN::INFRA::MigrationRunner;

namespace
{
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
        recorded.push_back(migration);
        return SZX::COMMON::Result<void>::Success();
    }

    auto acquireMigrationLock(std::string_view, std::uint32_t) -> SZX::COMMON::Result<bool> override
    {
        ++acquireLockCount;
        return SZX::COMMON::Result<bool>::Success(lockAcquired);
    }

    auto releaseMigrationLock(std::string_view) -> SZX::COMMON::Result<void> override
    {
        ++releaseLockCount;
        return SZX::COMMON::Result<void>::Success();
    }

    auto begin() -> SZX::COMMON::Result<void> override
    {
        ++beginCount;
        return SZX::COMMON::Result<void>::Success();
    }

    auto commit() -> SZX::COMMON::Result<void> override
    {
        ++commitCount;
        return SZX::COMMON::Result<void>::Success();
    }

    auto rollback() -> SZX::COMMON::Result<void> override
    {
        ++rollbackCount;
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

TEST(MigrationRunnerTest, AppliesPendingMigrationAndRecordsIt)
{
    const std::string sql = "CREATE TABLE test_table (name VARCHAR(32)); INSERT INTO test_table(name) VALUES ('semi;colon');";
    const MigrationDefinition migration{1, sql, "checksum"};

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
    const MigrationDefinition migration{1, sql, "checksum"};

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
    const MigrationDefinition migration{1, sql, "checksum"};

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
    const MigrationDefinition migration{1, sql, "checksum"};

    FakeDatabaseConnection connection;
    connection.failOn = "FAIL THIS STATEMENT";
    const auto result = MigrationRunner{}.run(connection, std::span{&migration, 1});

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::MigrationFailed));
    EXPECT_EQ(connection.beginCount, 0);
    EXPECT_EQ(connection.commitCount, 0);
    EXPECT_EQ(connection.rollbackCount, 0);
}
