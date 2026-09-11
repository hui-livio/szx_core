#pragma once

#include "Result.hpp"
#include "Serializer.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::INFRA
{
struct DatabaseConfig final
{
    std::string host{"127.0.0.1"};
    std::uint16_t port{3306};
    std::string database;
    std::string username;
    std::string password;
    std::uint16_t poolSize{8};
    std::uint16_t connectTimeoutSeconds{5};
    std::uint16_t lockWaitTimeoutSeconds{5};

    [[nodiscard]] auto validate() const -> SZX::COMMON::Result<void>;
};
SZX_REFL(DatabaseConfig, host, port, database, username, password, poolSize, connectTimeoutSeconds, lockWaitTimeoutSeconds);

struct AppliedMigration final
{
    std::uint32_t version{};
    std::string checksum;
};

struct MigrationSummary final
{
    std::uint32_t currentVersion{};
    std::uint32_t appliedCount{};
};

class DatabaseConnection
{
public:
    virtual ~DatabaseConnection() = default;

    [[nodiscard]] virtual auto executeMigrationStatement(std::string_view sql) -> SZX::COMMON::Result<void>    = 0;
    [[nodiscard]] virtual auto loadAppliedMigrations() -> SZX::COMMON::Result<std::vector<AppliedMigration>>   = 0;
    [[nodiscard]] virtual auto recordMigration(const AppliedMigration &migration) -> SZX::COMMON::Result<void> = 0;
    [[nodiscard]] virtual auto acquireMigrationLock(std::string_view name, std::uint32_t timeoutSeconds)
        -> SZX::COMMON::Result<bool>                                                                    = 0;
    [[nodiscard]] virtual auto releaseMigrationLock(std::string_view name) -> SZX::COMMON::Result<void> = 0;
    [[nodiscard]] virtual auto begin() -> SZX::COMMON::Result<void>                                     = 0;
    [[nodiscard]] virtual auto commit() -> SZX::COMMON::Result<void>                                    = 0;
    [[nodiscard]] virtual auto rollback() -> SZX::COMMON::Result<void>                                  = 0;
};

class DatabasePool final
{
public:
    using ConnectionCallback = std::function<SZX::COMMON::Result<void>(DatabaseConnection &)>;

    explicit DatabasePool(DatabaseConfig config);
    ~DatabasePool();

    DatabasePool(const DatabasePool &)                     = delete;
    auto operator=(const DatabasePool &) -> DatabasePool & = delete;

    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto ping() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto withConnection(const ConnectionCallback &callback) -> SZX::COMMON::Result<void>;

private:
    class Impl;
    Impl *impl_{};
};

class MigrationRunner final
{
public:
    [[nodiscard]] auto run(DatabaseConnection &connection, const std::filesystem::path &directory)
        -> SZX::COMMON::Result<MigrationSummary>;
};
} // namespace SZX::ADMIN::INFRA
