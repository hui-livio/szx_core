#include "SzxAdmin/Infra/Database.h"
#include "OrmConnection.h"

#include "AuditRepository.h"
#include "BehaviorEventRepository.h"
#include "DatabaseInitialization.h"
#include "LogMaintenanceRepository.h"
#include "Logger.h"
#include "LoginRepository.h"
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
    std::string username;
    std::string username_canonical;
    std::string display_name;
    std::string password_hash;
    std::string role;
    std::string status;
    bool force_password_change{};
};
// Deliberately project only initialized columns. Nullable identities and timestamps
// are supplied by MySQL defaults, rather than empty strings.
YLT_REFL(users, id, username, username_canonical, display_name, password_hash, role, status, force_password_change);

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
        logMaintenanceRepository_(connection)
    {
    }

    auto executeMigrationStatement(std::string_view sql) -> SZX::COMMON::Result<void> override
    {
        if (sql.empty() || connection_.execute(std::string{sql}))
            return SZX::COMMON::Result<void>::Success();
        return databaseFailure(ErrorCode::DatabaseQueryFailed, "Database statement execution failed", {});
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
        const auto rows =
            connection_.query_s<users>("(username_canonical = ? OR role = ?) ORDER BY (role = ?) DESC, id LIMIT 1 FOR UPDATE",
                                       std::string{initial.username}, std::string{initial.role}, std::string{initial.role});
        if (connection_.has_error())
            return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Failure(
                static_cast<int>(ErrorCode::DatabaseQueryFailed), "Failed to read bootstrap administrator.");
        if (rows.empty())
            return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Success(std::nullopt);
        const auto &row = rows.front();
        return SZX::COMMON::Result<std::optional<BootstrapAdministrator>>::Success(
            BootstrapAdministrator{row.id, row.username, row.password_hash, row.role, row.status, row.force_password_change});
    }

    auto insertBootstrapAdministrator(const BootstrapAdministrator &administrator) -> SZX::COMMON::Result<void> override
    {
        const auto &initial = DETAIL::initialAdministrator();
        if (administrator.username != initial.username || administrator.role != initial.role ||
            administrator.status != initial.status || administrator.forcePasswordChange != initial.forcePasswordChange ||
            administrator.passwordHash.empty())
            return failure(ErrorCode::ValidationError, "Invalid bootstrap administrator.");
        const users row{
            administrator.id,           administrator.username, administrator.username, std::string{initial.displayName},
            administrator.passwordHash, administrator.role,     administrator.status,   administrator.forcePasswordChange};
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

private:
    DETAIL::OrmConnection &connection_;
    DETAIL::MySqlLoginRepository loginRepository_;
    DETAIL::MySqlUserRepository userRepository_;
    DETAIL::MySqlAuditRepository auditRepository_;
    DETAIL::MySqlBehaviorEventRepository behaviorEventRepository_;
    DETAIL::MySqlLogMaintenanceRepository logMaintenanceRepository_;
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
        impl_->started = false;
        impl_->connections.clear();
        impl_->condition.notify_all();
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

    std::unique_lock<std::recursive_timed_mutex> ormLock;
    try
    {
        ormLock = std::unique_lock<std::recursive_timed_mutex>(DETAIL::ormMutex(), std::defer_lock);
        if (!ormLock.try_lock_until(acquisitionDeadline))
            return failure(ErrorCode::DatabaseUnavailable, "Unable to acquire database execution slot.");
    } catch (...)
    {
        return failure(ErrorCode::DatabaseUnavailable, "Unable to acquire database execution slot.");
    }

    std::unique_ptr<DETAIL::OrmConnection> connection;
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
        MySqlConnection wrapper(*connection, impl_->logs);
        try
        {
            const auto result = callback(wrapper);
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
