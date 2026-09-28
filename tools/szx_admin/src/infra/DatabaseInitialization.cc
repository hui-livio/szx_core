#include "DatabaseInitialization.h"
#include "../auth/PasswordHash.h"
#include "OrmConnection.h"

#include "Logger.h"
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

constexpr std::string_view kMigration4 = R"SZXSQL(CREATE TABLE IF NOT EXISTS security_audit_logs (
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
  CONSTRAINT chk_security_audit_result CHECK (result IN ('success', 'failure', 'denied')),
  INDEX idx_security_audit_time (created_at DESC, id ASC),
  INDEX idx_security_audit_actor_time (actor_user_id, created_at DESC, id ASC),
  INDEX idx_security_audit_target_time (target_user_id, created_at DESC, id ASC),
  INDEX idx_security_audit_event_time (event_name, created_at DESC, id ASC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS client_behavior_events (
  id CHAR(36) NOT NULL PRIMARY KEY,
  user_id CHAR(36) NOT NULL,
  event_name VARCHAR(96) NOT NULL,
  result VARCHAR(16) NOT NULL,
  request_id VARCHAR(128) NOT NULL,
  ip VARCHAR(45),
  user_agent VARCHAR(512),
  details_json JSON NOT NULL,
  created_at DATETIME(6) NOT NULL,
  persisted_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  CONSTRAINT chk_client_behavior_result CHECK (result IN ('success', 'failure')),
  CONSTRAINT fk_client_behavior_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT,
  INDEX idx_client_behavior_time (created_at DESC, id ASC),
  INDEX idx_client_behavior_user_time (user_id, created_at DESC, id ASC),
  INDEX idx_client_behavior_event_time (event_name, created_at DESC, id ASC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
)SZXSQL";

constexpr std::string_view kMigration5 = "DROP TABLE IF EXISTS user_logs;";

// RBAC catalog. Deterministic ids make retries safe after an implicit DDL commit.
constexpr std::string_view kMigration6 = R"SZXSQL(CREATE TABLE IF NOT EXISTS roles (
  id CHAR(36) NOT NULL PRIMARY KEY,
  code VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin NOT NULL,
  name VARCHAR(128) NOT NULL,
  description VARCHAR(512) NOT NULL DEFAULT '',
  system_key VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin,
  status VARCHAR(16) NOT NULL DEFAULT 'active',
  version INT UNSIGNED NOT NULL DEFAULT 1,
  authz_version INT UNSIGNED NOT NULL DEFAULT 1,
  created_by CHAR(36),
  updated_by CHAR(36),
  archived_at DATETIME(6),
  created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),
  CONSTRAINT uk_roles_code UNIQUE (code),
  CONSTRAINT uk_roles_system_key UNIQUE (system_key),
  CONSTRAINT chk_roles_status CHECK (status IN ('active', 'disabled', 'archived')),
  CONSTRAINT chk_roles_versions CHECK (version > 0 AND authz_version > 0),
  CONSTRAINT chk_roles_archive CHECK ((status = 'archived' AND archived_at IS NOT NULL) OR
                                      (status <> 'archived' AND archived_at IS NULL)),
  CONSTRAINT fk_roles_created_by FOREIGN KEY (created_by) REFERENCES users(id),
  CONSTRAINT fk_roles_updated_by FOREIGN KEY (updated_by) REFERENCES users(id),
  KEY idx_roles_status_name (status, name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS permissions (
  id CHAR(36) NOT NULL PRIMARY KEY,
  code VARCHAR(96) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin NOT NULL,
  name VARCHAR(128) NOT NULL,
  description VARCHAR(512) NOT NULL DEFAULT '',
  module VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin NOT NULL,
  enforcement VARCHAR(16) NOT NULL,
  source VARCHAR(16) NOT NULL,
  status VARCHAR(16) NOT NULL DEFAULT 'active',
  required_for_super_admin BOOLEAN NOT NULL DEFAULT FALSE,
  version INT UNSIGNED NOT NULL DEFAULT 1,
  created_by CHAR(36),
  updated_by CHAR(36),
  archived_at DATETIME(6),
  created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6),
  CONSTRAINT uk_permissions_code UNIQUE (code),
  CONSTRAINT chk_permissions_enforcement CHECK (enforcement IN ('server', 'client')),
  CONSTRAINT chk_permissions_source CHECK (source IN ('system', 'custom')),
  CONSTRAINT chk_permissions_status CHECK (status IN ('active', 'disabled', 'archived')),
  CONSTRAINT chk_permissions_version CHECK (version > 0),
  CONSTRAINT chk_permissions_archive CHECK ((status = 'archived' AND archived_at IS NOT NULL) OR
                                            (status <> 'archived' AND archived_at IS NULL)),
  CONSTRAINT fk_permissions_created_by FOREIGN KEY (created_by) REFERENCES users(id),
  CONSTRAINT fk_permissions_updated_by FOREIGN KEY (updated_by) REFERENCES users(id),
  KEY idx_permissions_status_module (status, module, code),
  KEY idx_permissions_source_enforcement (source, enforcement)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS role_permissions (
  role_id CHAR(36) NOT NULL,
  permission_id CHAR(36) NOT NULL,
  assigned_by CHAR(36),
  assigned_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (role_id, permission_id),
  CONSTRAINT fk_role_permissions_role FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE RESTRICT,
  CONSTRAINT fk_role_permissions_permission FOREIGN KEY (permission_id) REFERENCES permissions(id) ON DELETE RESTRICT,
  CONSTRAINT fk_role_permissions_assigned_by FOREIGN KEY (assigned_by) REFERENCES users(id) ON DELETE SET NULL,
  KEY idx_role_permissions_permission (permission_id, role_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS role_manage_scopes (
  manager_role_id CHAR(36) NOT NULL,
  target_role_id CHAR(36) NOT NULL,
  assigned_by CHAR(36),
  assigned_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (manager_role_id, target_role_id),
  CONSTRAINT fk_role_scopes_manager FOREIGN KEY (manager_role_id) REFERENCES roles(id) ON DELETE RESTRICT,
  CONSTRAINT fk_role_scopes_target FOREIGN KEY (target_role_id) REFERENCES roles(id) ON DELETE RESTRICT,
  CONSTRAINT fk_role_scopes_assigned_by FOREIGN KEY (assigned_by) REFERENCES users(id) ON DELETE SET NULL,
  KEY idx_role_scopes_target (target_role_id, manager_role_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT IGNORE INTO roles (id, code, name, description, system_key) VALUES
 ('00000000-0000-4000-8000-000000000001', 'operator', 'Operator', 'Business operator', 'operator'),
 ('00000000-0000-4000-8000-000000000002', 'engineer', 'Engineer', 'Engineering operator', 'engineer'),
 ('00000000-0000-4000-8000-000000000003', 'admin', 'Administrator', 'User administrator', 'admin'),
 ('00000000-0000-4000-8000-000000000004', 'super_admin', 'Super administrator', 'System administrator', 'super_admin');

INSERT IGNORE INTO permissions (id, code, name, module, enforcement, source, required_for_super_admin) VALUES
 ('10000000-0000-4000-8000-000000000001', 'business.read', 'Read business data', 'business', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000002', 'business.operate', 'Operate business data', 'business', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000003', 'engineering.configure', 'Configure engineering', 'engineering', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000004', 'user.read', 'Read users', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000005', 'user.create', 'Create users', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000006', 'user.update', 'Update users', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000007', 'user.status', 'Change user status', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000008', 'user.unlock', 'Unlock users', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000009', 'user.password.reset', 'Reset user passwords', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000010', 'user.role.assign', 'Assign user roles', 'user', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000011', 'audit.read', 'Read audit logs', 'audit', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000012', 'behavior.read', 'Read behavior events', 'behavior', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000013', 'role.read', 'Read roles', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000014', 'role.create', 'Create roles', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000015', 'role.update', 'Update roles', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000016', 'role.status', 'Change role status', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000017', 'role.archive', 'Archive roles', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000018', 'role.permission.assign', 'Assign role permissions', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000019', 'role.scope.assign', 'Assign role scopes', 'role', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000020', 'permission.read', 'Read permissions', 'permission', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000021', 'permission.create', 'Create client permissions', 'permission', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000022', 'permission.update', 'Update permissions', 'permission', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000023', 'permission.status', 'Change permission status', 'permission', 'server', 'system', TRUE),
 ('10000000-0000-4000-8000-000000000024', 'permission.archive', 'Archive permissions', 'permission', 'server', 'system', TRUE);

INSERT IGNORE INTO role_permissions (role_id, permission_id)
 SELECT r.id, p.id FROM roles r JOIN permissions p
 WHERE r.code = 'operator' AND p.code IN ('business.read', 'business.operate');
INSERT IGNORE INTO role_permissions (role_id, permission_id)
 SELECT r.id, p.id FROM roles r JOIN permissions p
 WHERE r.code = 'engineer' AND p.code IN ('business.read', 'business.operate', 'engineering.configure');
INSERT IGNORE INTO role_permissions (role_id, permission_id)
 SELECT r.id, p.id FROM roles r JOIN permissions p
 WHERE r.code = 'admin' AND p.code IN ('business.read', 'business.operate', 'engineering.configure', 'user.read', 'user.create',
                                       'user.update', 'user.status', 'user.unlock', 'user.password.reset', 'audit.read', 'behavior.read');
INSERT IGNORE INTO role_permissions (role_id, permission_id)
 SELECT r.id, p.id FROM roles r JOIN permissions p WHERE r.code = 'super_admin';

INSERT IGNORE INTO role_manage_scopes (manager_role_id, target_role_id)
 SELECT manager.id, target.id FROM roles manager JOIN roles target
 WHERE (manager.code = 'admin' AND target.code IN ('operator', 'engineer')) OR manager.code = 'super_admin';
)SZXSQL";

// Identity migration is additive: old columns remain available while the
// one-time migration into the formal RBAC and identity model.
constexpr std::string_view kMigration7 = R"SZXSQL(ALTER TABLE users
  ADD COLUMN role_id CHAR(36) NULL AFTER role,
  ADD COLUMN login_identifier VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL AFTER username,
  ADD COLUMN login_identifier_canonical VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL AFTER login_identifier,
  ADD COLUMN identifier_type VARCHAR(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL AFTER login_identifier_canonical,
  ADD KEY idx_users_role_id (role_id),
  ADD KEY idx_users_login_identifier_canonical (login_identifier_canonical),
  ADD CONSTRAINT fk_users_role_id FOREIGN KEY (role_id) REFERENCES roles(id);

UPDATE users u JOIN roles r ON r.code = u.role
SET u.role_id = r.id
WHERE u.role_id IS NULL;

UPDATE users
SET login_identifier = CASE WHEN uid IS NOT NULL THEN uid ELSE username END,
    login_identifier_canonical = CASE WHEN uid IS NOT NULL THEN uid_canonical ELSE username_canonical END,
    identifier_type = CASE WHEN uid IS NOT NULL THEN 'uid' ELSE 'username' END
WHERE login_identifier IS NULL;
)SZXSQL";

// Enforce the single login-identifier namespace only after migration 7 has
// populated and readiness has verified every formal identity projection.
constexpr std::string_view kMigration8 = R"SZXSQL(ALTER TABLE users
  DROP INDEX idx_users_login_identifier_canonical,
  ADD UNIQUE KEY uk_users_login_identifier_canonical (login_identifier_canonical);
)SZXSQL";

// V1.2 is the only supported runtime schema. Remove the pre-V1.2 identity
// columns after the formal projection has been validated and indexed.
constexpr std::string_view kMigration9 = R"SZXSQL(ALTER TABLE users
  DROP INDEX uid_canonical,
  DROP INDEX username_canonical,
  DROP INDEX uk_users_login_identifier_canonical,
  DROP COLUMN uid,
  DROP COLUMN uid_canonical,
  DROP COLUMN username,
  DROP COLUMN username_canonical,
  MODIFY login_identifier VARCHAR(64) NOT NULL,
  MODIFY login_identifier_canonical VARCHAR(128) NOT NULL,
  MODIFY identifier_type VARCHAR(16) NOT NULL;
)SZXSQL";

constexpr MigrationDefinition kMigrations[] = {
    {1, kMigration1, "86a4ac820cc8457a2131d0400d4968268576b17113f38fe86b6beceea48c2047"},
    {2, kMigration2, "f4223aadebf005f145941d06b89d2d41e964f4c059d2f765a8e1b3678d389d2d"},
    {3, kMigration3, "66db48cd7957f45317269dcad13d2c2ac5c47eeacfc8e8bb715d2ca4b7e5f065"},
    {4, kMigration4, "193446d82dbee660610cbbc8f822469529d18dbf8760ddcffbb3c1d356e8236a"},
    {5, kMigration5, "df5a342ab4da5db2515c8469d3fe17751a6570cc6ee9b009bef9bed9d2463241"},
    {6, kMigration6, "ce30e1ba660394557f1ad3ae3eaeb043d265f685a3cfec6c00ef785b024c6e3e"},
    {7, kMigration7, "870047693dce68a887ef633fd3b08dc164e7ddd64d9da3ffab1131465b32b6be"},
    {8, kMigration8, "3ff6dc5983213744cceb8879b5af8f2522147cd42768843272178526b643cc72"},
    {9, kMigration9, "54fd8a5eb7048d97a65e421e5965d01739829bffde4196a5b7f32f4c1cd923e8"},
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
        LOGM_INFO("szx_admin.database", "event=ensure_database_begin host={} port={} database={} username={} timeout_seconds={}",
                  config.host, config.port, config.database, config.username, config.connectTimeoutSeconds);
        const auto valid = config.validate();
        if (!valid.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=ensure_database_config_invalid code={} message={}", valid.error,
                       valid.message);
            return valid;
        }
        DETAIL::OrmConnection connection;
        if (!connection.connect(config.host, config.username, config.password, "", static_cast<int>(config.connectTimeoutSeconds),
                                static_cast<int>(config.port)))
        {
            LOGM_ERROR("szx_admin.database", "event=server_connect_failed host={} port={} username={} driver_error={}",
                       config.host, config.port, config.username, connection.lastError());
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseUnavailable),
                                                      "Database server connection failed.");
        }
        LOGM_DEBUG("szx_admin.database", "event=server_connected host={} port={} username={}", config.host, config.port,
                   config.username);
        const auto statement =
            "CREATE DATABASE IF NOT EXISTS `" + config.database + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci";
        if (!connection.execute(statement))
        {
            LOGM_ERROR("szx_admin.database", "event=database_create_failed database={} driver_error={}", config.database,
                       connection.lastError());
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                      "Database creation failed.");
        }
        LOGM_INFO("szx_admin.database", "event=ensure_database_complete database={}", config.database);
        return SZX::COMMON::Result<void>::Success();
    } catch (...)
    {
        LOGM_ERROR("szx_admin.database", "event=ensure_database_exception database={}", config.database);
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
        LOGM_INFO("szx_admin.database", "event=migrations_begin definition_count={}", definitions.size());
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
        LOGM_DEBUG("szx_admin.database", "event=migration_lock_acquire name={} timeout_seconds=10", lockName);
        const auto acquired = connection.acquireMigrationLock(lockName, 10);
        if (!acquired.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=migration_lock_failed name={} code={} message={}", lockName, acquired.error,
                       acquired.message);
            return SZX::COMMON::Result<MigrationSummary>::Failure(acquired.error, acquired.message);
        }
        if (!acquired.value)
        {
            LOGM_ERROR("szx_admin.database", "event=migration_lock_busy name={}", lockName);
            return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                  "Migration is being executed by another instance.");
        }
        LOGM_DEBUG("szx_admin.database", "event=migration_lock_acquired name={}", lockName);
        MigrationLockGuard lock(connection, lockName);
        lock.lock();
        const auto schema = connection.executeMigrationStatement(kCreateMigrationJournal);
        if (!schema.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=migration_journal_failed code={} message={}", schema.error, schema.message);
            return SZX::COMMON::Result<MigrationSummary>::Failure(schema.error, schema.message);
        }
        const auto applied = connection.loadAppliedMigrations();
        if (!applied.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=migration_journal_read_failed code={} message={}", applied.error,
                       applied.message);
            return SZX::COMMON::Result<MigrationSummary>::Failure(applied.error, applied.message);
        }
        LOGM_INFO("szx_admin.database", "event=migration_journal_loaded applied_count={}", applied.value.size());
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
            {
                LOGM_DEBUG("szx_admin.database", "event=migration_skip version={} reason=already_applied", migration.version);
                continue;
            }
            LOGM_INFO("szx_admin.database", "event=migration_apply_begin version={}", migration.version);
            std::size_t statementIndex{};
            for (const auto &statement : splitStatements(migration.sql))
            {
                const auto result = connection.executeMigrationStatement(statement);
                if (!result.ok)
                {
                    LOGM_ERROR("szx_admin.database",
                               "event=migration_statement_failed version={} statement_index={} code={} message={}",
                               migration.version, statementIndex, result.error, result.message);
                    return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                          result.message);
                }
                ++statementIndex;
            }
            LOGM_DEBUG("szx_admin.database", "event=migration_statements_applied version={} statement_count={}",
                       migration.version, statementIndex);
            TransactionGuard transaction(connection);
            const auto started = connection.begin();
            if (!started.ok)
            {
                LOGM_ERROR("szx_admin.database", "event=migration_transaction_begin_failed version={} code={} message={}",
                           migration.version, started.error, started.message);
                return SZX::COMMON::Result<MigrationSummary>::Failure(started.error, started.message);
            }
            transaction.started();
            const auto journal = connection.recordMigration(AppliedMigration{migration.version, std::string{migration.checksum}});
            if (!journal.ok)
            {
                LOGM_ERROR("szx_admin.database", "event=migration_record_failed version={} code={} message={}", migration.version,
                           journal.error, journal.message);
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                      journal.message);
            }
            const auto committed = transaction.commit();
            if (!committed.ok)
            {
                LOGM_ERROR("szx_admin.database", "event=migration_commit_failed version={} code={} message={}", migration.version,
                           committed.error, committed.message);
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                                      committed.message);
            }
            LOGM_INFO("szx_admin.database", "event=migration_apply_complete version={}", migration.version);
            ++appliedCount;
        }
        LOGM_INFO("szx_admin.database", "event=migrations_complete current_version={} applied_count={}", latest, appliedCount);
        return SZX::COMMON::Result<MigrationSummary>::Success(MigrationSummary{latest, appliedCount});
    }

    catch (...)
    {
        LOGM_ERROR("szx_admin.database", "event=migrations_exception");
        return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                              "Migration execution threw an exception.");
    }
}

auto BootstrapAdminService::run(DatabaseConnection &connection) -> SZX::COMMON::Result<bool>
{
    try
    {
        constexpr std::string_view lockName = "szx_rbac_bootstrap";
        LOGM_INFO("szx_admin.database", "event=bootstrap_begin username={} role={}", kInitialAdministrator.username,
                  kInitialAdministrator.role);
        LOGM_DEBUG("szx_admin.database", "event=bootstrap_lock_acquire name={} timeout_seconds=10", lockName);
        const auto acquired = connection.acquireMigrationLock(lockName, 10);
        if (!acquired.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_lock_failed name={} code={} message={}", lockName, acquired.error,
                       acquired.message);
            return SZX::COMMON::Result<bool>::Failure(acquired.error, acquired.message);
        }
        if (!acquired.value)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_lock_busy name={}", lockName);
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseTransactionFailed),
                                                      "Bootstrap initialization is being executed by another instance.");
        }
        LOGM_DEBUG("szx_admin.database", "event=bootstrap_lock_acquired name={}", lockName);
        MigrationLockGuard lock(connection, lockName);
        lock.lock();
        TransactionGuard transaction(connection);
        const auto started = connection.begin();
        if (!started.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_transaction_begin_failed code={} message={}", started.error,
                       started.message);
            return SZX::COMMON::Result<bool>::Failure(started.error, started.message);
        }
        transaction.started();
        const auto completed = connection.bootstrapCompleted();
        if (!completed.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_state_read_failed code={} message={}", completed.error,
                       completed.message);
            return SZX::COMMON::Result<bool>::Failure(completed.error, completed.message);
        }
        LOGM_DEBUG("szx_admin.database", "event=bootstrap_state_read completed={}", completed.value);
        if (completed.value)
        {
            LOGM_INFO("szx_admin.database", "event=bootstrap_skip reason=already_completed");
            return SZX::COMMON::Result<bool>::Success(false);
        }
        const auto existing = connection.findBootstrapAdministrator();
        if (!existing.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_account_lookup_failed code={} message={}", existing.error,
                       existing.message);
            return SZX::COMMON::Result<bool>::Failure(existing.error, existing.message);
        }
        if (existing.value)
        {
            LOGM_INFO("szx_admin.database", "event=bootstrap_account_found role={}", existing.value->role);
            if (existing.value->role != kInitialAdministrator.role)
            {
                LOGM_ERROR("szx_admin.database", "event=bootstrap_account_conflict role={}", existing.value->role);
                return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::UserIdentityConflict),
                                                          "The bootstrap username belongs to a non-super-admin user.");
            }
            const auto marked = connection.recordBootstrapCompletion(existing.value->id);
            if (!marked.ok)
            {
                LOGM_ERROR("szx_admin.database", "event=bootstrap_completion_record_failed code={} message={}", marked.error,
                           marked.message);
                return SZX::COMMON::Result<bool>::Failure(marked.error, marked.message);
            }
            const auto committed = transaction.commit();
            if (!committed.ok)
            {
                LOGM_ERROR("szx_admin.database", "event=bootstrap_existing_commit_failed code={} message={}", committed.error,
                           committed.message);
                return SZX::COMMON::Result<bool>::Failure(committed.error, committed.message);
            }
            LOGM_INFO("szx_admin.database", "event=bootstrap_reconciled account_id={}", existing.value->id);
            return SZX::COMMON::Result<bool>::Success(false);
        }
        const auto password = kInitialAdministrator.password;
        SZX::CRYPTO_SERVICE::CryptoService cryptoService;
        const auto clientHash = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256, password);
        if (!clientHash.ok)
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        const auto hash = AUTH::storeClientPasswordHash(cryptoService, clientHash.value);
        if (!hash.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_password_hash_failed code={} message={}", hash.error, hash.message);
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::InternalFailure),
                                                      "Bootstrap password hashing failed.");
        }
        const auto id       = SZX::COMMON::Uuid::v4().toString();
        const auto inserted = connection.insertBootstrapAdministrator(BootstrapAdministrator{
            id, std::string{kInitialAdministrator.username}, hash.value, std::string{kInitialAdministrator.role},
            std::string{kInitialAdministrator.status}, kInitialAdministrator.forcePasswordChange});
        if (!inserted.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_account_insert_failed code={} message={}", inserted.error,
                       inserted.message);
            return SZX::COMMON::Result<bool>::Failure(inserted.error, inserted.message);
        }
        const auto marked = connection.recordBootstrapCompletion(id);
        if (!marked.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_completion_record_failed code={} message={}", marked.error,
                       marked.message);
            return SZX::COMMON::Result<bool>::Failure(marked.error, marked.message);
        }
        const auto committed = transaction.commit();
        if (!committed.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=bootstrap_commit_failed code={} message={}", committed.error,
                       committed.message);
            return SZX::COMMON::Result<bool>::Failure(committed.error, committed.message);
        }
        LOGM_INFO("szx_admin.database", "event=bootstrap_created account_id={} username={} role={}", id,
                  kInitialAdministrator.username, kInitialAdministrator.role);
        return SZX::COMMON::Result<bool>::Success(true);
    } catch (...)
    {
        LOGM_ERROR("szx_admin.database", "event=bootstrap_exception");
        return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                  "Bootstrap administrator initialization failed.");
    }
}
} // namespace SZX::ADMIN::INFRA
