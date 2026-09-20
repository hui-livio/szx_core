#include "DatabaseInitialization.h"
#include "../auth/PasswordHash.h"
#include "OrmConnection.h"

#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
#include "SzxCryptoService/CryptoService.h"
#include "UUID.hpp"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SZX::ADMIN::INFRA
{
namespace
{
// Initialization catalog: keep all schema SQL and seed account values here.
// Published migration SQL is immutable (including whitespace); append new versions.
constexpr std::string_view kCreateMigrationJournal =
    "CREATE TABLE IF NOT EXISTS schema_migrations (version INT UNSIGNED NOT NULL PRIMARY KEY, checksum CHAR(64) NOT NULL, applied_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

constexpr std::string_view kMigration1 = R"SZXSQL(CREATE TABLE IF NOT EXISTS users (
  id CHAR(36) NOT NULL PRIMARY KEY,
  uid VARCHAR(64),
  uid_canonical VARCHAR(64) UNIQUE,
  username VARCHAR(64),
  username_canonical VARCHAR(64) UNIQUE,
  display_name VARCHAR(128) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  role VARCHAR(16) NOT NULL,
  status VARCHAR(16) NOT NULL DEFAULT 'active',
  version INT NOT NULL DEFAULT 1,
  force_password_change BOOLEAN NOT NULL DEFAULT TRUE,
  temporary_credential_expires_at DATETIME(6),
  failed_login_count INT NOT NULL DEFAULT 0,
  locked_until DATETIME(6),
  password_changed_at DATETIME(6),
  disabled_at DATETIME(6),
  created_by CHAR(36),
  authz_version INT NOT NULL DEFAULT 1,
  last_login_at DATETIME(6),
  created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),
  CONSTRAINT chk_users_role CHECK (role IN ('operator', 'engineer', 'admin', 'super_admin')),
  CONSTRAINT chk_users_status CHECK (status IN ('active', 'disabled', 'locked')),
  CONSTRAINT chk_users_identity CHECK ((uid IS NOT NULL) <> (username IS NOT NULL)),
  CONSTRAINT chk_users_uid_canonical CHECK ((uid IS NULL) = (uid_canonical IS NULL)),
  CONSTRAINT chk_users_username_canonical CHECK ((username IS NULL) = (username_canonical IS NULL)),
  CONSTRAINT chk_users_failed_login_count CHECK (failed_login_count >= 0),
  CONSTRAINT chk_users_versions CHECK (version > 0 AND authz_version > 0),
  CONSTRAINT fk_users_created_by FOREIGN KEY (created_by) REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS refresh_tokens (
  id CHAR(36) NOT NULL PRIMARY KEY,
  user_id CHAR(36) NOT NULL,
  token_hash VARCHAR(128) NOT NULL UNIQUE,
  family_id CHAR(36) NOT NULL,
  replaced_by CHAR(36),
  created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  last_used_at DATETIME(6),
  expires_at DATETIME(6) NOT NULL,
  revoked_at DATETIME(6),
  revocation_reason VARCHAR(64),
  ip VARCHAR(45),
  user_agent VARCHAR(512),
  CONSTRAINT fk_refresh_tokens_user FOREIGN KEY (user_id) REFERENCES users(id),
  CONSTRAINT fk_refresh_tokens_replaced_by FOREIGN KEY (replaced_by) REFERENCES refresh_tokens(id),
  INDEX idx_refresh_tokens_user (user_id, created_at),
  INDEX idx_refresh_tokens_family (family_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS user_logs (
  id CHAR(36) NOT NULL PRIMARY KEY,
  actor_user_id CHAR(36),
  target_user_id CHAR(36),
  event_name VARCHAR(96) NOT NULL,
  result VARCHAR(16) NOT NULL,
  request_id VARCHAR(128) NOT NULL,
  ip VARCHAR(45),
  user_agent VARCHAR(512),
  details_json JSON NOT NULL,
  created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  CONSTRAINT chk_user_logs_result CHECK (result IN ('success', 'failure', 'denied')),
  CONSTRAINT fk_user_logs_actor FOREIGN KEY (actor_user_id) REFERENCES users(id),
  CONSTRAINT fk_user_logs_target FOREIGN KEY (target_user_id) REFERENCES users(id),
  INDEX idx_user_logs_actor_time (actor_user_id, created_at),
  INDEX idx_user_logs_target_time (target_user_id, created_at),
  INDEX idx_user_logs_event_time (event_name, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
)SZXSQL";

constexpr std::string_view kMigration2 = R"SZXSQL(CREATE TABLE IF NOT EXISTS admin_bootstrap (
  id INT UNSIGNED NOT NULL PRIMARY KEY,
  user_id CHAR(36) NOT NULL,
  completed_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  CONSTRAINT chk_admin_bootstrap_singleton CHECK (id = 1)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
)SZXSQL";

constexpr std::string_view kMigration3 = R"SZXSQL(ALTER TABLE users
  MODIFY uid_canonical VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin NULL,
  MODIFY username_canonical VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin NULL;
)SZXSQL";

constexpr MigrationDefinition kMigrations[] = {
    {1, kMigration1, "86a4ac820cc8457a2131d0400d4968268576b17113f38fe86b6beceea48c2047"},
    {2, kMigration2, "f4223aadebf005f145941d06b89d2d41e964f4c059d2f765a8e1b3678d389d2d"},
    {3, kMigration3, "66db48cd7957f45317269dcad13d2c2ac5c47eeacfc8e8bb715d2ca4b7e5f065"},
};

constexpr DETAIL::InitialAdministratorDefinition kInitialAdministrator{
    .username            = "admin",
    .password            = "superadmin",
    .displayName         = "System Administrator",
    .role                = "super_admin",
    .status              = "active",
    .forcePasswordChange = true,
};

auto splitStatements(std::string_view sql) -> std::vector<std::string>
{
    std::vector<std::string> statements;
    std::string current;
    bool singleQuote  = false;
    bool doubleQuote  = false;
    bool backtick     = false;
    bool lineComment  = false;
    bool blockComment = false;

    for (std::size_t index = 0; index < sql.size(); ++index)
    {
        const auto character = sql[index];
        const auto next      = index + 1 < sql.size() ? sql[index + 1] : '\0';

        if (lineComment)
        {
            if (character == '\n')
                lineComment = false;
            current.push_back(character);
            continue;
        }
        if (blockComment)
        {
            if (character == '*' && next == '/')
            {
                current.append("*/");
                ++index;
                blockComment = false;
            }
            else
            {
                current.push_back(character);
            }
            continue;
        }
        if (!singleQuote && !doubleQuote && !backtick && character == '-' && next == '-')
        {
            current.append("--");
            ++index;
            lineComment = true;
            continue;
        }
        if (!singleQuote && !doubleQuote && !backtick && character == '#')
        {
            current.push_back(character);
            lineComment = true;
            continue;
        }
        if (!singleQuote && !doubleQuote && !backtick && character == '/' && next == '*')
        {
            current.append("/*");
            ++index;
            blockComment = true;
            continue;
        }
        if (character == '\\' && (singleQuote || doubleQuote) && index + 1 < sql.size())
        {
            current.push_back(character);
            current.push_back(sql[++index]);
            continue;
        }
        if (character == '\'' && !doubleQuote && !backtick)
            singleQuote = !singleQuote;
        else if (character == '"' && !singleQuote && !backtick)
            doubleQuote = !doubleQuote;
        else if (character == '`' && !singleQuote && !doubleQuote)
            backtick = !backtick;

        if (character == ';' && !singleQuote && !doubleQuote && !backtick)
        {
            if (current.find_first_not_of(" \t\r\n") != std::string::npos)
                statements.push_back(std::move(current));
            current.clear();
        }
        else
        {
            current.push_back(character);
        }
    }
    if (current.find_first_not_of(" \t\r\n") != std::string::npos)
        statements.push_back(std::move(current));
    return statements;
}

class MigrationLockGuard final
{
public:
    MigrationLockGuard(DatabaseConnection &connection, std::string_view name) :
        connection_(connection),
        name_(name)
    {
    }

    ~MigrationLockGuard()
    {
        if (locked_)
        {
            try
            {
                (void)connection_.releaseMigrationLock(name_);
            } catch (...)
            {
            } // Cleanup must not escape during transaction unwinding.
        }
    }

    void lock()
    {
        locked_ = true;
    }

private:
    DatabaseConnection &connection_;
    std::string_view name_;
    bool locked_{};
};

// Construct after the advisory lock so rollback always precedes lock release.
class TransactionGuard final
{
public:
    explicit TransactionGuard(DatabaseConnection &connection) noexcept :
        connection_(connection)
    {
    }
    ~TransactionGuard()
    {
        if (active_)
        {
            try
            {
                (void)connection_.rollback();
            } catch (...)
            {
            }
        }
    }
    void started() noexcept
    {
        active_ = true;
    }
    auto commit() -> SZX::COMMON::Result<void>
    {
        const auto result = connection_.commit();
        if (result.ok)
            active_ = false;
        return result;
    }
    TransactionGuard(const TransactionGuard &)                     = delete;
    auto operator=(const TransactionGuard &) -> TransactionGuard & = delete;

private:
    DatabaseConnection &connection_;
    bool active_{};
};
} // namespace

auto DETAIL::initialAdministrator() noexcept -> const InitialAdministratorDefinition &
{
    return kInitialAdministrator;
}

auto DETAIL::ensureDatabase(const DatabaseConfig &config) -> SZX::COMMON::Result<void>
{
    try
    {
        const auto valid = config.validate();
        if (!valid.ok)
            return valid;
        DETAIL::OrmConnection connection;
        if (!connection.connect(config.host, config.username, config.password, "", static_cast<int>(config.connectTimeoutSeconds),
                                static_cast<int>(config.port)))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseUnavailable),
                                                      "Database server connection failed.");
        const auto statement =
            "CREATE DATABASE IF NOT EXISTS `" + config.database + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci";
        if (!connection.execute(statement))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                      "Database creation failed.");
        return SZX::COMMON::Result<void>::Success();
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseUnavailable),
                                                  "Database server initialization failed.");
    }
}

auto embeddedMigrations() noexcept -> std::span<const MigrationDefinition>
{
    return kMigrations;
}

auto MigrationRunner::run(DatabaseConnection &connection, std::span<const MigrationDefinition> definitions)
    -> SZX::COMMON::Result<MigrationSummary>
{
    try
    {
        std::unordered_map<std::uint32_t, std::string_view> supported;
        std::uint32_t latest{};
        for (const auto &migration : definitions)
        {
            if (migration.version == 0 || migration.version <= latest || migration.sql.empty() || migration.checksum.empty())
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                      "Invalid embedded migration definition.");
            const auto bytes =
                std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(migration.sql.data()), migration.sql.size()};
            const auto digest = SZX::CRYPTO::sha256KeyId(bytes);
            if (!digest.ok)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                      "Failed to calculate migration checksum.");
            if (digest.value != migration.checksum)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationConflict),
                                                                      "Embedded migration checksum mismatch.");
            supported.emplace(migration.version, migration.checksum);
            latest = migration.version;
        }

        constexpr std::string_view lockName = "szx_rbac_schema_migrations";
        const auto acquired                 = connection.acquireMigrationLock(lockName, 10);
        if (!acquired.ok)
            return SZX::COMMON::Result<MigrationSummary>::Failure(acquired.error, acquired.message);
        if (!acquired.value)
            return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                  "Migration is being executed by another instance.");
        MigrationLockGuard lock(connection, lockName);
        lock.lock();
        const auto schema = connection.executeMigrationStatement(kCreateMigrationJournal);
        if (!schema.ok)
            return SZX::COMMON::Result<MigrationSummary>::Failure(schema.error, schema.message);
        const auto applied = connection.loadAppliedMigrations();
        if (!applied.ok)
            return SZX::COMMON::Result<MigrationSummary>::Failure(applied.error, applied.message);
        std::unordered_map<std::uint32_t, std::string> recorded;
        for (const auto &migration : applied.value)
        {
            if (!recorded.emplace(migration.version, migration.checksum).second)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationConflict),
                                                                      "schema_migrations contains duplicate versions.");
            const auto expected = supported.find(migration.version);
            if (expected == supported.end())
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationConflict),
                                                                      "Database contains an unsupported migration version.");
            if (expected->second != migration.checksum)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationConflict),
                                                                      "Checksum mismatch for an applied migration.");
        }
        std::uint32_t appliedCount{};
        for (const auto &migration : definitions)
        {
            const auto recordedMigration = recorded.find(migration.version);
            if (recordedMigration != recorded.end())
                continue;
            for (const auto &statement : splitStatements(migration.sql))
            {
                const auto result = connection.executeMigrationStatement(statement);
                if (!result.ok)
                    return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                          result.message);
            }
            TransactionGuard transaction(connection);
            const auto started = connection.begin();
            if (!started.ok)
                return SZX::COMMON::Result<MigrationSummary>::Failure(started.error, started.message);
            transaction.started();
            const auto journal = connection.recordMigration(AppliedMigration{migration.version, std::string{migration.checksum}});
            if (!journal.ok)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                      journal.message);
            const auto committed = transaction.commit();
            if (!committed.ok)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                      committed.message);
            ++appliedCount;
        }
        return SZX::COMMON::Result<MigrationSummary>::Success(MigrationSummary{latest, appliedCount});
    }

    catch (...)
    {
        return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                              "Migration execution threw an exception.");
    }
}

auto BootstrapAdminService::run(DatabaseConnection &connection) -> SZX::COMMON::Result<bool>
{
    try
    {
        constexpr std::string_view lockName = "szx_rbac_bootstrap";
        const auto acquired                 = connection.acquireMigrationLock(lockName, 10);
        if (!acquired.ok)
            return SZX::COMMON::Result<bool>::Failure(acquired.error, acquired.message);
        if (!acquired.value)
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseTransactionFailed),
                                                      "Bootstrap initialization is being executed by another instance.");
        MigrationLockGuard lock(connection, lockName);
        lock.lock();
        TransactionGuard transaction(connection);
        const auto started = connection.begin();
        if (!started.ok)
            return SZX::COMMON::Result<bool>::Failure(started.error, started.message);
        transaction.started();
        const auto completed = connection.bootstrapCompleted();
        if (!completed.ok)
            return SZX::COMMON::Result<bool>::Failure(completed.error, completed.message);
        if (completed.value)
            return SZX::COMMON::Result<bool>::Success(false);
        const auto existing = connection.findBootstrapAdministrator();
        if (!existing.ok)
            return SZX::COMMON::Result<bool>::Failure(existing.error, existing.message);
        if (existing.value)
        {
            if (existing.value->role != kInitialAdministrator.role)
                return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::UserIdentityConflict),
                                                          "The bootstrap username belongs to a non-super-admin user.");
            const auto marked = connection.recordBootstrapCompletion(existing.value->id);
            if (!marked.ok)
                return SZX::COMMON::Result<bool>::Failure(marked.error, marked.message);
            const auto committed = transaction.commit();
            if (!committed.ok)
                return SZX::COMMON::Result<bool>::Failure(committed.error, committed.message);
            return SZX::COMMON::Result<bool>::Success(false);
        }
        const auto password = kInitialAdministrator.password;
        SZX::CRYPTO_SERVICE::CryptoService cryptoService;
        const auto clientHash = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, password);
        if (!clientHash.ok)
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        const auto hash = AUTH::storeClientPasswordHash(cryptoService, clientHash.value);
        if (!hash.ok)
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::InternalFailure),
                                                      "Bootstrap password hashing failed.");
        const auto id       = SZX::COMMON::Uuid::v4().toString();
        const auto inserted = connection.insertBootstrapAdministrator(BootstrapAdministrator{
            id, std::string{kInitialAdministrator.username}, hash.value, std::string{kInitialAdministrator.role},
            std::string{kInitialAdministrator.status}, kInitialAdministrator.forcePasswordChange});
        if (!inserted.ok)
            return SZX::COMMON::Result<bool>::Failure(inserted.error, inserted.message);
        const auto marked = connection.recordBootstrapCompletion(id);
        if (!marked.ok)
            return SZX::COMMON::Result<bool>::Failure(marked.error, marked.message);
        const auto committed = transaction.commit();
        if (!committed.ok)
            return SZX::COMMON::Result<bool>::Failure(committed.error, committed.message);
        return SZX::COMMON::Result<bool>::Success(true);
    } catch (...)
    {
        return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                  "Bootstrap administrator initialization failed.");
    }
}
} // namespace SZX::ADMIN::INFRA
