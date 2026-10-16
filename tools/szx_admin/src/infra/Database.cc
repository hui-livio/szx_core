#include "SzxAdmin/Infra/Database.h"
#include "OrmConnection.h"

#include "../rbac/PermissionCodes.h"
#include "AuditRepository.h"
#include "BehaviorEventRepository.h"
#include "DatabaseInitialization.h"
#include "LogMaintenanceRepository.h"
#include "Logger.h"
#include "LoginRepository.h"
#include "PermissionRepository.h"
#include "RoleRepository.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "UserRepository.h"

#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace SZX::ADMIN::INFRA
{
namespace
{
struct schema_migrations final
{
    std::uint32_t version{};
    std::string checksum;
};
YLT_REFL(schema_migrations, version, checksum);
struct admin_bootstrap final
{
    std::uint32_t id{};
    std::string user_id;
};
YLT_REFL(admin_bootstrap, id, user_id);
struct users final
{
    std::string id;
    std::string login_identifier;
    std::string login_identifier_canonical;
    std::string identifier_type;
    std::optional<std::string> role_id;
    std::string display_name;
    std::string password_hash;
    std::string role;
    std::string status;
    bool force_password_change{};
};
// Deliberately project only initialized columns. Nullable identities and timestamps
// are supplied by MySQL defaults, rather than empty strings.
YLT_REFL(users, id, login_identifier, login_identifier_canonical, identifier_type, role_id, display_name, password_hash, role,
         status, force_password_change);

auto failure(ErrorCode code, const std::string &message) -> SZX::COMMON::Result<void>
{
    return SZX::COMMON::Result<void>::Failure(static_cast<int>(code), message);
}

auto databaseFailure(ErrorCode code, std::string_view operation, std::string_view detail) -> SZX::COMMON::Result<void>
{
    (void)detail;
    return failure(code, std::string{operation} + "。");
}

auto connectAndConfigure(const DatabaseConfig &config, DETAIL::OrmConnection &connection) -> SZX::COMMON::Result<void>
{
    LOGM_DEBUG("szx_admin.database", "event=connection_attempt host={} port={} database={} username={} timeout_seconds={}",
               config.host, config.port, config.database, config.username, config.connectTimeoutSeconds);
    if (!connection.connect(config.host, config.username, config.password, config.database,
                            static_cast<int>(config.connectTimeoutSeconds), static_cast<int>(config.port)))
    {
        LOGM_ERROR("szx_admin.database", "event=connection_failed host={} port={} database={} username={} driver_error={}",
                   config.host, config.port, config.database, config.username, connection.lastError());
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database connection failed", {});
    }
    LOGM_DEBUG("szx_admin.database", "event=connection_established host={} port={} database={} username={}", config.host,
               config.port, config.database, config.username);

    constexpr std::array<std::string_view, 2> sessionStatements{
        "SET SESSION time_zone = '+00:00'",
        "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED",
    };
    constexpr std::array<std::string_view, 2> sessionNames{"time_zone", "transaction_isolation"};
    for (std::size_t index = 0; index < sessionStatements.size(); ++index)
    {
        if (!connection.execute(std::string{sessionStatements[index]}))
        {
            LOGM_ERROR("szx_admin.database",
                       "event=session_initialization_failed setting={} host={} port={} database={} driver_error={}",
                       sessionNames[index], config.host, config.port, config.database, connection.lastError());
            return databaseFailure(ErrorCode::DatabaseUnavailable, "Database session initialization failed", {});
        }
        LOGM_DEBUG("szx_admin.database", "event=session_initialized setting={}", sessionNames[index]);
    }
    if (!connection.execute("SET SESSION innodb_lock_wait_timeout = " + std::to_string(config.lockWaitTimeoutSeconds)))
    {
        LOGM_ERROR(
            "szx_admin.database",
            "event=session_initialization_failed setting=innodb_lock_wait_timeout host={} port={} database={} driver_error={}",
            config.host, config.port, config.database, connection.lastError());
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database session initialization failed", {});
    }
    LOGM_DEBUG("szx_admin.database", "event=session_initialized setting=innodb_lock_wait_timeout");
    if (!connection.execute("SET SESSION net_read_timeout = " + std::to_string(config.connectTimeoutSeconds) +
                            ", net_write_timeout = " + std::to_string(config.connectTimeoutSeconds)))
    {
        LOGM_ERROR("szx_admin.database",
                   "event=session_initialization_failed setting=network_timeout host={} port={} database={} driver_error={}",
                   config.host, config.port, config.database, connection.lastError());
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database session initialization failed", {});
    }
    LOGM_DEBUG("szx_admin.database", "event=session_initialized setting=network_timeout");
    return SZX::COMMON::Result<void>::Success();
}

auto connectAndConfigureUntil(const DatabaseConfig &config, DETAIL::OrmConnection &connection,
                              std::chrono::steady_clock::time_point deadline) -> SZX::COMMON::Result<void>
{
    const auto remaining = deadline - std::chrono::steady_clock::now();
    if (remaining < std::chrono::seconds{1})
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database connection deadline exceeded", {});
    auto boundedConfig          = config;
    const auto remainingSeconds = std::chrono::duration_cast<std::chrono::seconds>(remaining).count();
    boundedConfig.connectTimeoutSeconds =
        static_cast<std::uint16_t>((std::min)(static_cast<std::int64_t>(config.connectTimeoutSeconds), remainingSeconds));
    return connectAndConfigure(boundedConfig, connection);
}

auto validDatabaseIdentifier(std::string_view value) noexcept -> bool
{
    if (value.empty() || value.size() > 64)
        return false;
    for (const auto character : value)
    {
        if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_' && character != '$')
            return false;
    }
    return true;
}

class MySqlConnection final : public DatabaseConnection
{
public:
    explicit MySqlConnection(DETAIL::OrmConnection &connection, const LogStorageConfig &logs) :
        connection_(connection),
        loginRepository_(connection),
        userRepository_(connection),
        auditRepository_(connection),
        behaviorEventRepository_(connection),
        logMaintenanceRepository_(connection),
        roleRepository_(connection),
        permissionRepository_(connection)
    {
    }

    auto executeMigrationStatement(std::string_view sql) -> SZX::COMMON::Result<void> override
    {
        try
        {
            constexpr std::string_view roleColumnMigration = "ALTER TABLE users\n  DROP CHECK chk_users_role,\n";
            std::string recovered;
            if (sql.starts_with(roleColumnMigration))
            {
                const auto constraints = connection_.query_s<std::tuple<std::uint64_t>>(
                    "SELECT COUNT(*) FROM information_schema.TABLE_CONSTRAINTS "
                    "WHERE CONSTRAINT_SCHEMA = DATABASE() AND TABLE_NAME = 'users' "
                    "AND CONSTRAINT_NAME = 'chk_users_role' AND CONSTRAINT_TYPE = 'CHECK'");
                if (connection_.has_error() || constraints.size() != 1)
                    return databaseFailure(ErrorCode::DatabaseQueryFailed, "Migration schema inspection failed", {});
                if (std::get<0>(constraints.front()) == 0)
                {
                    recovered = "ALTER TABLE users\n";
                    recovered.append(sql.substr(roleColumnMigration.size()));
                    sql = recovered;
                }
            }
            if (sql.empty() || connection_.execute(DETAIL::initializationSql(sql, connection_.dialect())))
                return SZX::COMMON::Result<void>::Success();
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Database statement execution failed", {});
        } catch (...)
        {
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Database statement execution failed", {});
        }
    }

    auto loadAppliedMigrations() -> SZX::COMMON::Result<std::vector<AppliedMigration>> override
    {
        const auto rows = connection_.query_s<schema_migrations>("ORDER BY version");
        if (connection_.has_error())
        {
            return SZX::COMMON::Result<std::vector<AppliedMigration>>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                                               "Failed to read migration records.");
        }

        std::vector<AppliedMigration> result;
        result.reserve(rows.size());
        for (const auto &row : rows)
            result.push_back(AppliedMigration{row.version, row.checksum});
        return SZX::COMMON::Result<std::vector<AppliedMigration>>::Success(std::move(result));
    }

    auto recordMigration(const AppliedMigration &migration) -> SZX::COMMON::Result<void> override
    {
        const auto affected = connection_.insert(schema_migrations{migration.version, migration.checksum});
        if (affected == INT_MIN || connection_.has_error())
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Failed to record migration", {});
        return SZX::COMMON::Result<void>::Success();
    }

    auto acquireMigrationLock(std::string_view name, std::uint32_t timeoutSeconds) -> SZX::COMMON::Result<bool> override
    {
        const auto rows =
            connection_.query_s<std::tuple<int>>("SELECT GET_LOCK(?, ?)", std::string{name}, static_cast<int>(timeoutSeconds));
        if (connection_.has_error())
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                      "Failed to acquire migration lock.");
        if (rows.empty())
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                      "Failed to acquire migration lock.");
        return SZX::COMMON::Result<bool>::Success(std::get<0>(rows.front()) == 1);
    }

    auto releaseMigrationLock(std::string_view name) -> SZX::COMMON::Result<void> override
    {
        const auto rows = connection_.query_s<std::tuple<int>>("SELECT RELEASE_LOCK(?)", std::string{name});
        if (connection_.has_error() || rows.empty() || std::get<0>(rows.front()) != 1)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                      "Failed to release migration lock.");
        return SZX::COMMON::Result<void>::Success();
    }

    auto begin() -> SZX::COMMON::Result<void> override
    {
        return connection_.begin() ?
            SZX::COMMON::Result<void>::Success() :
            databaseFailure(ErrorCode::DatabaseTransactionFailed, "Failed to begin database transaction", {});
    }

    auto commit() -> SZX::COMMON::Result<void> override
    {
        return connection_.commit() ?
            SZX::COMMON::Result<void>::Success() :
            databaseFailure(ErrorCode::DatabaseTransactionFailed, "Failed to commit database transaction", {});
    }

    auto commitWithOutcome() -> SZX::COMMON::Result<CommitOutcome> override
    {
        if (connection_.commit())
            return SZX::COMMON::Result<CommitOutcome>::Success(CommitOutcome::Committed);
        connection_.invalidate();
        return SZX::COMMON::Result<CommitOutcome>::Success(CommitOutcome::Unknown);
    }

    auto rollback() -> SZX::COMMON::Result<void> override
    {
        return connection_.rollback() ?
            SZX::COMMON::Result<void>::Success() :
            databaseFailure(ErrorCode::DatabaseTransactionFailed, "Failed to roll back database transaction", {});
    }

    void invalidate() noexcept override
    {
        connection_.invalidate();
    }

    auto reusable() const noexcept -> bool override
    {
        return connection_.reusable();
    }

    auto bootstrapCompleted() -> SZX::COMMON::Result<bool> override
    {
        const auto rows = connection_.query_s<admin_bootstrap>("id = ? FOR UPDATE", 1);
        if (connection_.has_error())
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed),
                                                      "Failed to read bootstrap state.");
        return SZX::COMMON::Result<bool>::Success(!rows.empty());
    }

    auto findBootstrapAdministrator() -> SZX::COMMON::Result<std::optional<BootstrapAdministrator>> override
    {
        const auto &initial = DETAIL::initialAdministrator();
        const auto rows     = connection_.query_s<users>(
            "(login_identifier_canonical = ? OR role = ?) ORDER BY (role = ?) DESC, id LIMIT 1 FOR UPDATE",
            std::string{initial.username}, std::string{initial.role}, std::string{initial.role});
        if (connection_.has_error())
            return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Failure(
                static_cast<int>(ErrorCode::DatabaseQueryFailed), "Failed to read bootstrap administrator.");
        if (rows.empty())
            return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Success(std::nullopt);
        const auto &row = rows.front();
        return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Success(BootstrapAdministrator{
            row.id, row.login_identifier, row.password_hash, row.role, row.status, row.force_password_change});
    }

    auto insertBootstrapAdministrator(const BootstrapAdministrator &administrator) -> SZX::COMMON::Result<void> override
    {
        const auto &initial = DETAIL::initialAdministrator();
        if (administrator.username != initial.username || administrator.role != initial.role ||
            administrator.status != initial.status || administrator.forcePasswordChange != initial.forcePasswordChange ||
            administrator.passwordHash.empty())
            return failure(ErrorCode::ValidationError, "Invalid bootstrap administrator.");
        const auto roles =
            connection_.query_s<std::tuple<std::string>>("SELECT id FROM roles WHERE code = ? LIMIT 1", administrator.role);
        if (connection_.has_error() || roles.size() != 1)
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Failed to resolve bootstrap role", {});
        const users row{
            administrator.id,           administrator.username,           administrator.username,     "username",
            std::get<0>(roles.front()), std::string{initial.displayName}, administrator.passwordHash, administrator.role,
            administrator.status,       administrator.forcePasswordChange};
        const auto affected = connection_.insert(row);
        if (affected != 1 || connection_.has_error())
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Failed to create bootstrap administrator", {});
        return SZX::COMMON::Result<void>::Success();
    }

    auto recordBootstrapCompletion(std::string_view userId) -> SZX::COMMON::Result<void> override
    {
        const auto affected = connection_.insert(admin_bootstrap{1, std::string{userId}});
        if (affected != 1 || connection_.has_error())
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Failed to record bootstrap state", {});
        return SZX::COMMON::Result<void>::Success();
    }

    auto loginRepository() noexcept -> AUTH::LoginRepository * override
    {
        return &loginRepository_;
    }

    auto userRepository() noexcept -> USER::UserRepository * override
    {
        return &userRepository_;
    }

    auto auditRepository() noexcept -> AUDIT::AuditRepository * override
    {
        return &auditRepository_;
    }

    auto behaviorEventRepository() noexcept -> TELEMETRY::BehaviorEventRepository * override
    {
        return &behaviorEventRepository_;
    }

    auto logMaintenanceRepository() noexcept -> MAINTENANCE::LogMaintenanceRepository * override
    {
        return &logMaintenanceRepository_;
    }

    auto roleRepository() noexcept -> RBAC::RoleRepository * override
    {
        return &roleRepository_;
    }

    auto permissionRepository() noexcept -> RBAC::PermissionRepository * override
    {
        return &permissionRepository_;
    }

    auto rbacCatalogReady() -> SZX::COMMON::Result<bool> override
    {
        try
        {
            const auto roles = connection_.query_s<std::tuple<std::uint64_t>>(
                "SELECT COUNT(*) FROM roles WHERE system_key IN (?, ?, ?, ?) AND status <> 'archived'", "operator", "engineer",
                "admin", "super_admin");
            if (connection_.has_error() || roles.size() != 1 || std::get<0>(roles.front()) != 4)
                return SZX::COMMON::Result<bool>::Success(false);

            const auto identityRows = connection_.query_s<std::tuple<std::uint64_t>>(
                "SELECT COUNT(*) FROM users u LEFT JOIN roles r ON r.id = u.role_id "
                "WHERE u.role_id IS NULL OR r.id IS NULL OR r.code <> u.role "
                "OR u.login_identifier IS NULL OR u.login_identifier_canonical IS NULL "
                "OR u.identifier_type NOT IN ('uid', 'username')");
            if (connection_.has_error() || identityRows.size() != 1)
                return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
            if (std::get<0>(identityRows.front()) != 0)
                return SZX::COMMON::Result<bool>::Success(false);

            const auto permissionRows = connection_.query_s<std::tuple<std::string>>(
                "SELECT code FROM permissions WHERE source = 'system' AND enforcement = 'server' AND status <> 'archived'");
            if (connection_.has_error())
                return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
            std::unordered_set<std::string> registered;
            registered.reserve(permissionRows.size());
            for (const auto &row : permissionRows)
                registered.emplace(std::get<0>(row));
            if (registered.size() != RBAC::PERMISSIONS::definitions().size())
                return SZX::COMMON::Result<bool>::Success(false);
            for (const auto &definition : RBAC::PERMISSIONS::definitions())
                if (!registered.contains(std::string{definition.code}))
                    return SZX::COMMON::Result<bool>::Success(false);

            const auto bottomLine = connection_.query_s<std::tuple<std::uint64_t>>(
                "SELECT COUNT(*) FROM role_permissions rp JOIN roles r ON r.id = rp.role_id "
                "JOIN permissions p ON p.id = rp.permission_id WHERE r.system_key = 'super_admin' "
                "AND p.required_for_super_admin = TRUE AND p.status <> 'archived'");
            if (connection_.has_error() || bottomLine.size() != 1 ||
                std::get<0>(bottomLine.front()) != RBAC::PERMISSIONS::definitions().size())
                return SZX::COMMON::Result<bool>::Success(false);

            const auto scopes = connection_.query_s<std::tuple<std::uint64_t>>(
                "SELECT COUNT(*) FROM role_manage_scopes scope JOIN roles manager ON manager.id = scope.manager_role_id "
                "JOIN roles target ON target.id = scope.target_role_id WHERE manager.system_key = 'super_admin' "
                "AND target.status <> 'archived'");
            if (connection_.has_error() || scopes.size() != 1)
                return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
            const auto nonArchived =
                connection_.query_s<std::tuple<std::uint64_t>>("SELECT COUNT(*) FROM roles WHERE status <> 'archived'");
            if (connection_.has_error() || nonArchived.size() != 1)
                return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
            return SZX::COMMON::Result<bool>::Success(std::get<0>(scopes.front()) == std::get<0>(nonArchived.front()));
        } catch (...)
        {
            return SZX::COMMON::Result<bool>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
        }
    }

private:
    DETAIL::OrmConnection &connection_;
    DETAIL::MySqlLoginRepository loginRepository_;
    DETAIL::MySqlUserRepository userRepository_;
    DETAIL::MySqlAuditRepository auditRepository_;
    DETAIL::MySqlBehaviorEventRepository behaviorEventRepository_;
    DETAIL::MySqlLogMaintenanceRepository logMaintenanceRepository_;
    DETAIL::MySqlRoleRepository roleRepository_;
    DETAIL::MySqlPermissionRepository permissionRepository_;
};

} // namespace

class DatabasePool::Impl final
{
public:
    explicit Impl(DatabaseConfig value, LogStorageConfig logConfig) :
        config(std::move(value)),
        logs(std::move(logConfig))
    {
    }

    void reportConnectionState(bool available) noexcept
    {
        if (connectionUnavailable.exchange(!available) == !available)
            return;
        try
        {
            LOGM_INFO("szx_admin.database", "event={}", available ? "connection_recovered" : "connection_unavailable");
        } catch (...)
        {
        }
    }

    DatabaseConfig config;
    LogStorageConfig logs;
    bool started{};
    std::atomic_bool connectionUnavailable{};
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::unique_ptr<DETAIL::OrmConnection>> connections;
};

auto DatabaseConfig::validate() const -> SZX::COMMON::Result<void>
{
    if (host.empty() || host.size() > 253 || database.empty() || database.size() > 64 || username.empty() || username.size() > 64)
        return failure(ErrorCode::InvalidConfiguration, "Invalid database host, database, or username configuration.");
    if (!validDatabaseIdentifier(database))
        return failure(ErrorCode::InvalidConfiguration,
                       "Database name may contain only letters, digits, underscores, or dollar signs.");
    if (port == 0 || poolSize == 0 || poolSize > 64 || connectTimeoutSeconds == 0 || connectTimeoutSeconds > 120 ||
        lockWaitTimeoutSeconds == 0 || lockWaitTimeoutSeconds > 120)
        return failure(ErrorCode::InvalidConfiguration, "Invalid database port, pool, or timeout configuration.");
    return SZX::COMMON::Result<void>::Success();
}

auto LogStorageConfig::validate() const -> SZX::COMMON::Result<void>
{
    if (auditRetentionDays == 0 || behaviorRetentionDays == 0 || cleanupBatchSize == 0 || cleanupBatchSize > 100000)
        return failure(ErrorCode::InvalidConfiguration, "Invalid log retention or cleanup configuration.");
    return SZX::COMMON::Result<void>::Success();
}

DatabasePool::DatabasePool(DatabaseConfig config, LogStorageConfig logs) :
    impl_(new Impl(std::move(config), std::move(logs)))
{
}

DatabasePool::~DatabasePool()
{
    delete impl_;
}

auto DatabasePool::start() -> SZX::COMMON::Result<void>
{
    if (impl_ == nullptr)
        return failure(ErrorCode::InternalFailure, "Invalid database pool state.");
    const auto valid = impl_->config.validate();
    if (!valid.ok)
        return valid;
    const auto logsValid = impl_->logs.validate();
    if (!logsValid.ok)
        return logsValid;
    LOGM_INFO(
        "szx_admin.database",
        "event=database_start_begin host={} port={} database={} username={} pool_size={} timeout_seconds={} lock_wait_timeout_seconds={}",
        impl_->config.host, impl_->config.port, impl_->config.database, impl_->config.username, impl_->config.poolSize,
        impl_->config.connectTimeoutSeconds, impl_->config.lockWaitTimeoutSeconds);
    try
    {
        std::lock_guard ormLock(DETAIL::ormMutex());
        std::unique_lock lock(impl_->mutex);
        if (impl_->started)
            return SZX::COMMON::Result<void>::Success();
        const auto database = DETAIL::ensureDatabase(impl_->config);
        if (!database.ok)
        {
            LOGM_ERROR("szx_admin.database", "event=ensure_database_failed code={} message={}", database.error, database.message);
            return database;
        }
        LOGM_INFO("szx_admin.database", "event=database_available database={}", impl_->config.database);
        std::deque<std::unique_ptr<DETAIL::OrmConnection>> connections;
        for (std::uint16_t index = 0; index < impl_->config.poolSize; ++index)
        {
            auto connection      = std::make_unique<DETAIL::OrmConnection>();
            const auto connected = connectAndConfigure(impl_->config, *connection);
            if (!connected.ok)
            {
                LOGM_ERROR("szx_admin.database", "event=pool_connection_failed index={} code={} message={}", index,
                           connected.error, connected.message);
                return connected;
            }
            connections.push_back(std::move(connection));
            LOGM_DEBUG("szx_admin.database", "event=pool_connection_ready index={} total={}", index, connections.size());
        }
        impl_->connections = std::move(connections);
        impl_->started     = true;
        impl_->condition.notify_all();
        LOGM_INFO("szx_admin.database", "event=database_start_complete database={} pool_size={}", impl_->config.database,
                  impl_->connections.size());
        return SZX::COMMON::Result<void>::Success();
    } catch (const std::exception &error)
    {
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database connection failed", error.what());
    } catch (...)
    {
        return failure(ErrorCode::DatabaseUnavailable, "Database connection failed.");
    }
}

auto DatabasePool::stop() -> SZX::COMMON::Result<void>
{
    if (impl_ != nullptr)
    {
        std::lock_guard lock(impl_->mutex);
        if (!impl_->started)
            return SZX::COMMON::Result<void>::Success();
        impl_->started = false;
        impl_->connections.clear();
        impl_->condition.notify_all();
        LOGM_INFO("szx_admin.database", "event=database_stop_complete database={}", impl_->config.database);
    }
    return SZX::COMMON::Result<void>::Success();
}

auto DatabasePool::ping() -> SZX::COMMON::Result<void>
{
    return withConnection([](DatabaseConnection &) { return SZX::COMMON::Result<void>::Success(); });
}

auto DatabasePool::withConnection(const ConnectionCallback &callback) -> SZX::COMMON::Result<void>
{
    return withConnectionImpl(callback, std::chrono::steady_clock::now() + std::chrono::seconds{3}, std::nullopt);
}

auto DatabasePool::withConnectionUntil(const ConnectionCallback &callback, std::chrono::steady_clock::time_point deadline)
    -> SZX::COMMON::Result<void>
{
    return withConnectionImpl(callback, deadline, deadline);
}

auto DatabasePool::withConnectionImpl(const ConnectionCallback &callback,
                                      std::chrono::steady_clock::time_point acquisitionDeadline,
                                      std::optional<std::chrono::steady_clock::time_point> operationDeadline)
    -> SZX::COMMON::Result<void>
{
    if (impl_ == nullptr)
        return failure(ErrorCode::DatabaseUnavailable, "Database pool has not been started.");
    if (!callback)
        return failure(ErrorCode::InvalidConfiguration, "Database connection callback is empty.");

    std::unique_ptr<DETAIL::OrmConnection> connection;
    const auto acquireStarted = std::chrono::steady_clock::now();
    {
        std::unique_lock lock(impl_->mutex);
        if (!impl_->started)
            return failure(ErrorCode::DatabaseUnavailable, "Database pool has not been started.");
        if (!impl_->condition.wait_until(lock, acquisitionDeadline,
                                         [this] { return !impl_->started || !impl_->connections.empty(); }))
            return failure(ErrorCode::DatabaseUnavailable, "Unable to acquire a connection from the database pool.");
        if (!impl_->started)
            return failure(ErrorCode::DatabaseUnavailable, "Database pool has been stopped.");
        connection = std::move(impl_->connections.front());
        impl_->connections.pop_front();
    }
    const auto acquireElapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - acquireStarted);
    if (acquireElapsed >= std::chrono::milliseconds{100})
        LOGM_DEBUG("szx_admin.database", "event=pool_acquire_slow duration_microseconds={}", acquireElapsed.count());
    if (connection)
        connection->setDeadline(operationDeadline);

    const auto returnConnection = [this, &connection](bool reusable)
    {
        // A null slot retains capacity but can never reuse an uncertain transaction.
        if (!reusable)
        {
            impl_->reportConnectionState(false);
            connection.reset();
        }
        else if (connection)
            connection->setDeadline(std::nullopt);
        std::lock_guard lock(impl_->mutex);
        if (impl_->started)
            impl_->connections.push_back(std::move(connection));
        impl_->condition.notify_one();
    };

    std::unique_ptr<DETAIL::OrmConnection> staleConnection;
    try
    {
        if (!connection || !connection->ping())
        {
            // Empty slots already reported their failure when discarded.
            if (connection)
                impl_->reportConnectionState(false);
            staleConnection  = std::move(connection);
            auto replacement = std::make_unique<DETAIL::OrmConnection>();
            replacement->setDeadline(operationDeadline);
            const auto connected = operationDeadline ? connectAndConfigureUntil(impl_->config, *replacement, *operationDeadline) :
                                                       connectAndConfigure(impl_->config, *replacement);
            if (!connected.ok)
            {
                impl_->reportConnectionState(false);
                connection = std::move(staleConnection);
                returnConnection(true);
                return connected;
            }
            connection = std::move(replacement);
            impl_->reportConnectionState(true);
        }
    } catch (const std::exception &error)
    {
        if (staleConnection)
            connection = std::move(staleConnection);
        returnConnection(true);
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database reconnection threw an exception", error.what());
    } catch (...)
    {
        if (staleConnection)
            connection = std::move(staleConnection);
        returnConnection(true);
        return failure(ErrorCode::DatabaseUnavailable, "Database reconnection threw an exception.");
    }

    try
    {
        const auto callbackStarted = std::chrono::steady_clock::now();
        MySqlConnection wrapper(*connection, impl_->logs);
        try
        {
            const auto result = callback(wrapper);
            const auto callbackElapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - callbackStarted);
            if (callbackElapsed >= std::chrono::milliseconds{100})
                LOGM_DEBUG("szx_admin.database", "event=database_callback_slow duration_microseconds={}",
                           callbackElapsed.count());
            if (!wrapper.reusable())
            {
                returnConnection(false);
                return result;
            }
            const auto cleanup = wrapper.rollback();
            returnConnection(cleanup.ok && wrapper.reusable());
            if (!cleanup.ok)
                return cleanup;
            return result;
        } catch (...)
        {
            bool reusable = false;
            try
            {
                reusable = wrapper.rollback().ok;
            } catch (...)
            {
            }
            returnConnection(reusable);
            throw;
        }
    } catch (const std::exception &error)
    {
        return databaseFailure(ErrorCode::DatabaseQueryFailed, "Database operation threw an exception", error.what());
    } catch (...)
    {
        return failure(ErrorCode::DatabaseQueryFailed, "Database operation threw an exception.");
    }
}

} // namespace SZX::ADMIN::INFRA
