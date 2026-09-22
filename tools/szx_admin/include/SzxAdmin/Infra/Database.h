#pragma once

#include "Result.hpp"
#include "Serializer.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::AUTH
{
class LoginRepository;
}

namespace SZX::ADMIN::AUDIT
{
class AuditRepository;
}

namespace SZX::ADMIN::TELEMETRY
{
class BehaviorEventRepository;
}

namespace SZX::ADMIN::MAINTENANCE
{
class LogMaintenanceRepository;
}

namespace SZX::ADMIN::USER
{
class UserRepository;
}

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

struct LogStorageConfig final
{
    std::uint32_t auditRetentionDays{180};
    std::uint32_t behaviorRetentionDays{30};
    std::uint32_t cleanupBatchSize{5000};

    [[nodiscard]] auto validate() const -> SZX::COMMON::Result<void>;
};
SZX_REFL(LogStorageConfig, auditRetentionDays, behaviorRetentionDays, cleanupBatchSize);

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

enum class CommitOutcome : unsigned char
{
    Committed,
    NotCommitted,
    Unknown,
};

/** Immutable compiled migration. checksum is the SHA-256 of LF-normalized SQL. */
struct MigrationDefinition final
{
    std::uint32_t version{};
    std::string_view sql;
    std::string_view checksum;
};

[[nodiscard]] auto embeddedMigrations() noexcept -> std::span<const MigrationDefinition>;

/** Repository projection; initialization policy is owned by DatabaseInitialization.cc. */
struct BootstrapAdministrator final
{
    std::string id;
    std::string username;
    std::string passwordHash;
    std::string role;
    std::string status;
    bool forcePasswordChange{};
};

class DatabaseConnection
{
public:
    virtual ~DatabaseConnection() = default;

    /** Borrow the login repository for this connection's transaction lifetime. */
    [[nodiscard]] virtual auto loginRepository() noexcept -> AUTH::LoginRepository *
    {
        return nullptr;
    }

    /** Borrow the user repository for this connection's transaction lifetime. */
    [[nodiscard]] virtual auto userRepository() noexcept -> USER::UserRepository *
    {
        return nullptr;
    }

    /** Borrow the trusted audit repository for this connection's transaction lifetime. */
    [[nodiscard]] virtual auto auditRepository() noexcept -> AUDIT::AuditRepository *
    {
        return nullptr;
    }

    /** Borrow the client telemetry repository for this connection's transaction lifetime. */
    [[nodiscard]] virtual auto behaviorEventRepository() noexcept -> TELEMETRY::BehaviorEventRepository *
    {
        return nullptr;
    }

    [[nodiscard]] virtual auto logMaintenanceRepository() noexcept -> MAINTENANCE::LogMaintenanceRepository *
    {
        return nullptr;
    }

    [[nodiscard]] virtual auto executeMigrationStatement(std::string_view sql) -> SZX::COMMON::Result<void>    = 0;
    [[nodiscard]] virtual auto loadAppliedMigrations() -> SZX::COMMON::Result<std::vector<AppliedMigration>>   = 0;
    [[nodiscard]] virtual auto recordMigration(const AppliedMigration &migration) -> SZX::COMMON::Result<void> = 0;
    [[nodiscard]] virtual auto acquireMigrationLock(std::string_view name, std::uint32_t timeoutSeconds)
        -> SZX::COMMON::Result<bool>                                                                    = 0;
    [[nodiscard]] virtual auto releaseMigrationLock(std::string_view name) -> SZX::COMMON::Result<void> = 0;
    [[nodiscard]] virtual auto begin() -> SZX::COMMON::Result<void>                                     = 0;
    [[nodiscard]] virtual auto commit() -> SZX::COMMON::Result<void>                                    = 0;
    [[nodiscard]] virtual auto commitWithOutcome() -> SZX::COMMON::Result<CommitOutcome>
    {
        const auto result = commit();
        return result.ok ? SZX::COMMON::Result<CommitOutcome>::Success(CommitOutcome::Committed) :
                           SZX::COMMON::Result<CommitOutcome>::Success(CommitOutcome::NotCommitted);
    }
    [[nodiscard]] virtual auto rollback() -> SZX::COMMON::Result<void> = 0;
    virtual void invalidate() noexcept
    {
    }
    [[nodiscard]] virtual auto reusable() const noexcept -> bool
    {
        return true;
    }
    [[nodiscard]] virtual auto bootstrapCompleted() -> SZX::COMMON::Result<bool>
    {
        return SZX::COMMON::Result<bool>::Failure(2221, "Bootstrap repository is not implemented.");
    }
    [[nodiscard]] virtual auto findBootstrapAdministrator() -> SZX::COMMON::Result<std::optional<BootstrapAdministrator>>
    {
        return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Failure(2221,
                                                                                   "Bootstrap repository is not implemented.");
    }
    [[nodiscard]] virtual auto insertBootstrapAdministrator(const BootstrapAdministrator &administrator)
        -> SZX::COMMON::Result<void>
    {
        return SZX::COMMON::Result<void>::Failure(2221, "Bootstrap repository is not implemented.");
    }
    [[nodiscard]] virtual auto recordBootstrapCompletion(std::string_view) -> SZX::COMMON::Result<void>
    {
        return SZX::COMMON::Result<void>::Failure(2221, "Bootstrap repository is not implemented.");
    }
};

class DatabasePool final
{
public:
    using ConnectionCallback = std::function<SZX::COMMON::Result<void>(DatabaseConnection &)>;

    explicit DatabasePool(DatabaseConfig config, LogStorageConfig logs = {});
    ~DatabasePool();

    DatabasePool(const DatabasePool &)                     = delete;
    auto operator=(const DatabasePool &) -> DatabasePool & = delete;

    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto ping() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto withConnection(const ConnectionCallback &callback) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto withConnectionUntil(const ConnectionCallback &callback, std::chrono::steady_clock::time_point deadline)
        -> SZX::COMMON::Result<void>;

private:
    [[nodiscard]] auto withConnectionImpl(const ConnectionCallback &callback,
                                          std::chrono::steady_clock::time_point acquisitionDeadline,
                                          std::optional<std::chrono::steady_clock::time_point> operationDeadline)
        -> SZX::COMMON::Result<void>;
    class Impl;
    Impl *impl_{};
};

class MigrationRunner final
{
public:
    [[nodiscard]] auto run(DatabaseConnection &connection,
                           std::span<const MigrationDefinition> definitions = embeddedMigrations())
        -> SZX::COMMON::Result<MigrationSummary>;
};

class BootstrapAdminService final
{
public:
    /** Owns its transaction; call without an active transaction. Holds the initialization lock
     * through commit/rollback. Returns true only when created; never resets an existing account. */
    [[nodiscard]] auto run(DatabaseConnection &connection) -> SZX::COMMON::Result<bool>;
};
} // namespace SZX::ADMIN::INFRA
