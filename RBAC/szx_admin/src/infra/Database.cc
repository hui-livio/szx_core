#include "SzxAdmin/Infra/Database.h"

#include "FileHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxCrypto/Random.h"
#include "EmbeddedMigrations.h"

#include <ormpp/mysql.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <regex>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

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

auto failure(ErrorCode code, const std::string &message) -> SZX::COMMON::Result<void>
{
    return SZX::COMMON::Result<void>::Failure(static_cast<int>(code), message);
}

auto databaseFailure(ErrorCode code, std::string_view operation, std::string_view detail) -> SZX::COMMON::Result<void>
{
    (void)detail;
    return failure(code, std::string{operation} + "。");
}

auto connectAndConfigure(const DatabaseConfig &config, ormpp::mysql &connection) -> SZX::COMMON::Result<void>
{
    if (!connection.connect(config.host, config.username, config.password, config.database,
                            static_cast<int>(config.connectTimeoutSeconds), static_cast<int>(config.port)))
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database connection failed", connection.get_last_error());

    constexpr std::array<std::string_view, 2> sessionStatements{
        "SET SESSION time_zone = '+00:00'",
        "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED",
    };
    for (const auto statement : sessionStatements)
    {
        if (!connection.execute(std::string{statement}))
            return databaseFailure(ErrorCode::DatabaseUnavailable, "Database session initialization failed", connection.get_last_error());
    }
    if (!connection.execute("SET SESSION innodb_lock_wait_timeout = " + std::to_string(config.lockWaitTimeoutSeconds)))
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database session initialization failed", connection.get_last_error());
    return SZX::COMMON::Result<void>::Success();
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

auto ensureDatabase(const DatabaseConfig &config) -> SZX::COMMON::Result<void>
{
    try
    {
        ormpp::mysql connection;
        if (!connection.connect(config.host, config.username, config.password, "", static_cast<int>(config.connectTimeoutSeconds),
                                static_cast<int>(config.port)))
            return databaseFailure(ErrorCode::DatabaseUnavailable, "Database server connection failed", connection.get_last_error());

        const auto statement =
            "CREATE DATABASE IF NOT EXISTS `" + config.database + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci";
        if (!connection.execute(statement))
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Database creation failed", connection.get_last_error());
        return SZX::COMMON::Result<void>::Success();
    } catch (const std::exception &error)
    {
        return databaseFailure(ErrorCode::DatabaseUnavailable, "Database server connection threw an exception", error.what());
    } catch (...)
    {
        return failure(ErrorCode::DatabaseUnavailable, "Database server connection threw an exception.");
    }
}

auto readFile(const std::filesystem::path &path) -> SZX::COMMON::Result<std::string>
{
    const auto contents = SZX::COMMON::FileHelper::ReadText(path);
    if (!contents)
        return SZX::COMMON::Result<std::string>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                         "Unable to read migration file.");
    return SZX::COMMON::Result<std::string>::Success(*contents);
}

auto checksum(std::string_view text) -> SZX::COMMON::Result<std::string>
{
    const auto bytes  = std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(text.data()), text.size()};
    const auto result = SZX::CRYPTO::sha256KeyId(bytes);
    if (!result.ok)
        return SZX::COMMON::Result<std::string>::Failure(static_cast<int>(ErrorCode::MigrationFailed),
                                                         "Failed to calculate migration checksum.");
    return result;
}

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

class MySqlConnection final : public DatabaseConnection
{
public:
    explicit MySqlConnection(ormpp::mysql &connection) :
        connection_(connection)
    {
        connection_.set_enable_transaction(false);
    }

    auto executeMigrationStatement(std::string_view sql) -> SZX::COMMON::Result<void> override
    {
        if (sql.empty() || connection_.execute(std::string{sql}))
            return SZX::COMMON::Result<void>::Success();
        return databaseFailure(ErrorCode::DatabaseQueryFailed, "Database statement execution failed", connection_.get_last_error());
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
            return databaseFailure(ErrorCode::DatabaseQueryFailed, "Failed to record migration", connection_.get_last_error());
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
            databaseFailure(ErrorCode::DatabaseTransactionFailed, "Failed to begin database transaction", connection_.get_last_error());
    }

    auto commit() -> SZX::COMMON::Result<void> override
    {
        return connection_.commit() ?
            SZX::COMMON::Result<void>::Success() :
            databaseFailure(ErrorCode::DatabaseTransactionFailed, "Failed to commit database transaction", connection_.get_last_error());
    }

    auto rollback() -> SZX::COMMON::Result<void> override
    {
        return connection_.rollback() ?
            SZX::COMMON::Result<void>::Success() :
            databaseFailure(ErrorCode::DatabaseTransactionFailed, "Failed to roll back database transaction", connection_.get_last_error());
    }

private:
    ormpp::mysql &connection_;
};

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
            (void)connection_.releaseMigrationLock(name_);
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
} // namespace

class DatabasePool::Impl final
{
public:
    explicit Impl(DatabaseConfig value) :
        config(std::move(value))
    {
    }

    DatabaseConfig config;
    bool started{};
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::unique_ptr<ormpp::mysql>> connections;
};

auto DatabaseConfig::validate() const -> SZX::COMMON::Result<void>
{
    if (host.empty() || host.size() > 253 || database.empty() || database.size() > 64 || username.empty() || username.size() > 64)
        return failure(ErrorCode::InvalidConfiguration, "Invalid database host, database, or username configuration.");
    if (!validDatabaseIdentifier(database))
        return failure(ErrorCode::InvalidConfiguration, "Database name may contain only letters, digits, underscores, or dollar signs.");
    if (port == 0 || poolSize == 0 || poolSize > 64 || connectTimeoutSeconds == 0 || connectTimeoutSeconds > 120 ||
        lockWaitTimeoutSeconds == 0 || lockWaitTimeoutSeconds > 120)
        return failure(ErrorCode::InvalidConfiguration, "Invalid database port, pool, or timeout configuration.");
    return SZX::COMMON::Result<void>::Success();
}

DatabasePool::DatabasePool(DatabaseConfig config) :
    impl_(new Impl(std::move(config)))
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
    try
    {
        std::unique_lock lock(impl_->mutex);
        if (impl_->started)
            return SZX::COMMON::Result<void>::Success();
        const auto database = ensureDatabase(impl_->config);
        if (!database.ok)
            return database;
        std::deque<std::unique_ptr<ormpp::mysql>> connections;
        for (std::uint16_t index = 0; index < impl_->config.poolSize; ++index)
        {
            auto connection = std::make_unique<ormpp::mysql>();
            if (!connection->connect(impl_->config.host, impl_->config.username, impl_->config.password, impl_->config.database,
                                     static_cast<int>(impl_->config.connectTimeoutSeconds), static_cast<int>(impl_->config.port)))
            {
                return databaseFailure(ErrorCode::DatabaseUnavailable, "Database connection failed", connection->get_last_error());
            }
            connections.push_back(std::move(connection));
        }
        impl_->connections = std::move(connections);
        impl_->started     = true;
        impl_->condition.notify_all();
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
    if (impl_ == nullptr)
        return failure(ErrorCode::DatabaseUnavailable, "Database pool has not been started.");
    if (!callback)
        return failure(ErrorCode::InvalidConfiguration, "Database connection callback is empty.");

    std::unique_ptr<ormpp::mysql> connection;
    {
        std::unique_lock lock(impl_->mutex);
        if (!impl_->started)
            return failure(ErrorCode::DatabaseUnavailable, "Database pool has not been started.");
        if (!impl_->condition.wait_for(lock, std::chrono::seconds(3),
                                       [this] { return !impl_->started || !impl_->connections.empty(); }))
            return failure(ErrorCode::DatabaseUnavailable, "Unable to acquire a connection from the database pool.");
        if (!impl_->started)
            return failure(ErrorCode::DatabaseUnavailable, "Database pool has been stopped.");
        connection = std::move(impl_->connections.front());
        impl_->connections.pop_front();
    }

    const auto returnConnection = [this, &connection](bool reusable)
    {
        std::lock_guard lock(impl_->mutex);
        if (reusable && impl_->started)
            impl_->connections.push_back(std::move(connection));
        impl_->condition.notify_one();
    };

    std::unique_ptr<ormpp::mysql> staleConnection;
    try
    {
        if (!connection->ping())
        {
            staleConnection      = std::move(connection);
            auto replacement     = std::make_unique<ormpp::mysql>();
            const auto connected = connectAndConfigure(impl_->config, *replacement);
            if (!connected.ok)
            {
                connection = std::move(staleConnection);
                returnConnection(true);
                return connected;
            }
            connection = std::move(replacement);
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
        MySqlConnection wrapper(*connection);
        try
        {
            const auto result  = callback(wrapper);
            const auto cleanup = wrapper.rollback();
            returnConnection(cleanup.ok);
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

auto embeddedMigrations() noexcept -> std::span<const MigrationDefinition>
{
    return DETAIL::kMigrations;
}

auto MigrationRunner::run(DatabaseConnection &connection, std::span<const MigrationDefinition> definitions)
    -> SZX::COMMON::Result<MigrationSummary>
{
    try
    {
        constexpr std::string_view lockName = "szx_rbac_schema_migrations";
        const auto acquired = connection.acquireMigrationLock(lockName, 10);
        if (!acquired.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(acquired.error, acquired.message);
        if (!acquired.value) return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Migration is being executed by another instance.");
        MigrationLockGuard lock(connection, lockName); lock.lock();
        const auto schema = connection.executeMigrationStatement("CREATE TABLE IF NOT EXISTS schema_migrations (version INT UNSIGNED NOT NULL PRIMARY KEY, checksum CHAR(64) NOT NULL, applied_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci");
        if (!schema.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(schema.error, schema.message);
        const auto applied = connection.loadAppliedMigrations();
        if (!applied.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(applied.error, applied.message);
        std::unordered_map<std::uint32_t, std::string> recorded;
        for (const auto &migration : applied.value)
            if (!recorded.emplace(migration.version, migration.checksum).second)
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationConflict), "schema_migrations contains duplicate versions.");
        std::uint32_t latest{}; std::uint32_t appliedCount{};
        for (const auto &migration : definitions)
        {
            if (migration.version == 0 || migration.version <= latest || migration.sql.empty() || migration.checksum.empty())
                return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Invalid embedded migration definition.");
            latest = migration.version;
            const auto recordedMigration = recorded.find(migration.version);
            if (recordedMigration != recorded.end())
            {
                if (recordedMigration->second != migration.checksum)
                    return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationConflict), "Checksum mismatch for an applied migration.");
                continue;
            }
            for (const auto &statement : splitStatements(migration.sql))
            {
                const auto result = connection.executeMigrationStatement(statement);
                if (!result.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), result.message);
            }
            const auto started = connection.begin();
            if (!started.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(started.error, started.message);
            const auto journal = connection.recordMigration(AppliedMigration{migration.version, std::string{migration.checksum}});
            if (!journal.ok) { (void)connection.rollback(); return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), journal.message); }
            const auto committed = connection.commit();
            if (!committed.ok) { (void)connection.rollback(); return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), committed.message); }
            ++appliedCount;
        }
        return SZX::COMMON::Result<MigrationSummary>::Success(MigrationSummary{latest, appliedCount});
    }
    catch (...) { return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Migration execution threw an exception."); }
}

auto MigrationRunner::run(DatabaseConnection &connection, const std::filesystem::path &directory)
    -> SZX::COMMON::Result<MigrationSummary>
{
    try
    {
        if (!std::filesystem::is_directory(directory))
            return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Migration directory does not exist.");
        std::vector<MigrationDefinition> definitions;
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".sql")
                continue;
            const auto content = readFile(entry.path());
            if (!content.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(content.error, content.message);
            const auto stem = entry.path().filename().string();
            const auto separator = stem.find_first_of("_-");
            if (separator == std::string::npos) return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Invalid migration filename format.");
            std::uint32_t version{};
            const auto parsed = std::from_chars(stem.data(), stem.data() + separator, version);
            if (parsed.ec != std::errc{} || version == 0) return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Invalid migration version.");
            const auto digest = checksum(content.value);
            if (!digest.ok) return SZX::COMMON::Result<MigrationSummary>::Failure(digest.error, digest.message);
            definitions.push_back(MigrationDefinition{version, std::move(content.value), std::move(digest.value)});
        }
        std::sort(definitions.begin(), definitions.end(), [](const auto &left, const auto &right) { return left.version < right.version; });
        return run(connection, definitions);
    }
    catch (...) { return SZX::COMMON::Result<MigrationSummary>::Failure(static_cast<int>(ErrorCode::MigrationFailed), "Migration execution threw an exception."); }
}
} // namespace SZX::ADMIN::INFRA
