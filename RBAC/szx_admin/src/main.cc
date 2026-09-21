#include "Command.hpp"
#include "FileHelper.hpp"
#include "Logger.h"
#include "Routes.h"
#include "Serializer.hpp"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/ServerConfig.h"
#include "auth/LoginService.h"
#include "maintenance/LogMaintenanceService.h"
#include "telemetry/BehaviorEventWriter.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace
{
using SZX::ADMIN::INFRA::DatabaseConfig;
using SZX::ADMIN::INFRA::DatabasePool;
using SZX::ADMIN::INFRA::ErrorCode;
using SZX::ADMIN::INFRA::HttpTransport;
using SZX::ADMIN::INFRA::MigrationRunner;
using SZX::ADMIN::INFRA::ServerConfig;

std::atomic_bool g_stopRequested{};
std::atomic_bool g_writerResumeRequested{};

void requestStop(int) noexcept
{
    g_stopRequested.store(true, std::memory_order_relaxed);
}

void requestWriterResume(int) noexcept
{
    g_writerResumeRequested.store(true, std::memory_order_relaxed);
}

struct ServiceConfig final
{
    struct Logging final
    {
        std::string level{"debug"};
        std::string flushLevel{"trace"};
        std::string loggerName{"szx_admin"};
        std::string pattern{"[%Y-%m-%d %H:%M:%S.%e] [%5t|%5P] [%^%-4l%$] [%n] %v"};
        std::string filePath{"logs/szx_admin.log"};
        std::size_t maxFileSize{10U * 1024U * 1024U};
        std::size_t maxFiles{10U};
        bool truncate{false};
        bool console{false};
    } logging;
    ServerConfig server;
    DatabaseConfig database;
    SZX::ADMIN::INFRA::LogStorageConfig logs;
    SZX::ADMIN::AUTH::LoginConfig auth;
    SZX::ADMIN::TELEMETRY::TelemetryConfig telemetry;
};
SZX_REFL(ServiceConfig::Logging, level, flushLevel, loggerName, pattern, filePath, maxFileSize, maxFiles, truncate, console);
SZX_REFL(ServiceConfig, logging, server, database, logs, auth, telemetry);

auto logLevel(std::string_view value) -> std::optional<SZX::LOGGER::LogLevel>
{
    using SZX::LOGGER::LogLevel;
    if (value == "trace")
        return LogLevel::Trace;
    if (value == "debug")
        return LogLevel::Debug;
    if (value == "info")
        return LogLevel::Info;
    if (value == "warn")
        return LogLevel::Warn;
    if (value == "error")
        return LogLevel::Error;
    if (value == "critical")
        return LogLevel::Critical;
    if (value == "off")
        return LogLevel::Off;
    return std::nullopt;
}

auto initializeLogger(const ServiceConfig::Logging &config) -> bool
{
    const auto level      = logLevel(config.level);
    const auto flushLevel = logLevel(config.flushLevel);
    if (!level || !flushLevel || config.loggerName.empty() || config.pattern.empty() || config.maxFileSize == 0 ||
        config.maxFiles == 0)
        return false;

    SZX::LOGGER::LogOptions options;
    options.level         = *level;
    options.flush_level   = *flushLevel;
    options.logger_name   = config.loggerName.c_str();
    options.pattern       = config.pattern.c_str();
    options.file_path     = config.filePath.c_str();
    options.max_file_size = config.maxFileSize;
    options.max_files     = config.maxFiles;
    options.truncate      = config.truncate;
    options.console       = config.console;
    SZX::LOGGER::init(options);
    return true;
}

auto defaultConfigPath() -> std::filesystem::path
{
    const auto current = std::filesystem::current_path();
    const auto direct  = current / "config.yaml";
    return std::filesystem::exists(direct) ? direct : current / "config" / "config.yaml";
}

auto loadConfig(int argc, char **argv, ServiceConfig &config, std::string &maintenance, bool &confirmPurge) -> int
{
    SZX::COMMON::Command command{"SZX Admin service", "SZX Admin service"};
    std::string configArgument;
    command.add_option("-c,--config", configArgument, "YAML configuration file path");
    command.add_option("--maintenance", maintenance, "Run inspect, backfill, or purge and exit");
    command.add_flag("--confirm-purge", confirmPurge, "Confirm retention purge deletion");
    try
    {
        command.parse(argc, argv);

    } catch (const CLI::ParseError &error)
    {
        const auto exitCode = command.exit(error);
        return exitCode == 0 ? -1 : exitCode;
    }

    const auto path         = configArgument.empty() ? defaultConfigPath() : std::filesystem::path{configArgument};
    const auto resolvedPath = path.is_absolute() ? path : std::filesystem::current_path() / path;
    auto content            = SZX::COMMON::FileHelper::ReadText(resolvedPath);
    if (!content)
    {
        std::cerr << "cannot read configuration file: " << resolvedPath.string() << '\n';
        return 2;
    }

    // Iguana expects LF; preserve configuration edited with Windows CRLF line endings.
    for (auto position = content->find("\r\n"); position != std::string::npos; position = content->find("\r\n", position))
        content->erase(position, 1);
    std::error_code parseError;
    SZX::COMMON::Serializer::from_yaml(config, *content, parseError);
    if (parseError)
    {
        std::cerr << "cannot parse configuration file " << resolvedPath.string() << ": " << parseError.message() << '\n';
        return 2;
    }
    if (!initializeLogger(config.logging))
    {
        std::cerr << "invalid logging configuration in " << resolvedPath << '\n';
        return 2;
    }

    return 0;
}

template <typename Result>
auto reportFailure(std::string_view operation, const Result &result) -> int
{
    LOGM_ERROR("szx_admin", "{} failed ({}): {}", operation, result.error, result.message);
    return result.error == static_cast<int>(ErrorCode::InvalidConfiguration) ? 2 : 1;
}

auto runMaintenance(DatabasePool &database, const SZX::ADMIN::INFRA::LogStorageConfig &config, std::string_view operation,
                    bool confirmPurge) -> int
{
    using SZX::ADMIN::MAINTENANCE::LogMaintenanceService;
    LogMaintenanceService service(database, config);
    if (operation == "inspect")
    {
        const auto result = service.inspect();
        if (!result.ok)
            return reportFailure("log migration inspection", result);
        const auto &value = result.value;
        std::cout << SZX::COMMON::Json{{"legacy_rows", value.legacyRows},
                                       {"eligible_audit_rows", value.eligibleAuditRows},
                                       {"eligible_behavior_rows", value.eligibleBehaviorRows},
                                       {"legacy_unknown_rows", value.legacyUnknownRows},
                                       {"audit_rows", value.auditRows},
                                       {"behavior_rows", value.behaviorRows},
                                       {"missing_audit_rows", value.missingAuditRows},
                                       {"missing_behavior_rows", value.missingBehaviorRows},
                                       {"conflicting_audit_rows", value.conflictingAuditRows},
                                       {"conflicting_behavior_rows", value.conflictingBehaviorRows}}
                         .dump(2)
                  << '\n';
        return 0;
    }
    if (operation == "backfill")
    {
        const auto result = service.backfill();
        if (!result.ok)
            return reportFailure("log migration backfill", result);
        std::cout << SZX::COMMON::Json{{"audit_inserted", result.value.auditInserted},
                                       {"behavior_inserted", result.value.behaviorInserted},
                                       {"legacy_unknown_rows", result.value.after.legacyUnknownRows},
                                       {"missing_audit_rows", result.value.after.missingAuditRows},
                                       {"missing_behavior_rows", result.value.after.missingBehaviorRows}}
                         .dump(2)
                  << '\n';
        return 0;
    }
    if (operation == "purge")
    {
        if (!confirmPurge)
        {
            LOGM_ERROR("szx_admin", "log purge requires --confirm-purge");
            return 2;
        }
        const auto result = service.purge();
        if (!result.ok)
            return reportFailure("log retention purge", result);
        std::cout << SZX::COMMON::Json{{"audit_deleted", result.value.auditDeleted},
                                       {"audit_legacy_deleted", result.value.auditLegacyDeleted},
                                       {"behavior_deleted", result.value.behaviorDeleted},
                                       {"behavior_legacy_deleted", result.value.behaviorLegacyDeleted}}
                         .dump(2)
                  << '\n';
        return 0;
    }
    LOGM_ERROR("szx_admin", "unknown maintenance operation: {}", operation);
    return 2;
}

} // namespace

int main(int argc, char **argv)
{
    ServiceConfig options;
    std::string maintenance;
    bool confirmPurge{};
    const auto configResult = loadConfig(argc, argv, options, maintenance, confirmPurge);
    if (configResult != 0)
        return configResult < 0 ? 0 : configResult;

    const auto databaseValid = options.database.validate();
    if (!databaseValid.ok)
        return reportFailure("database configuration", databaseValid);
    const auto logsValid = options.logs.validate();
    if (!logsValid.ok)
        return reportFailure("log storage configuration", logsValid);
    if (maintenance.empty())
    {
        const auto serverValid = options.server.validate();
        if (!serverValid.ok)
            return reportFailure("server configuration", serverValid);
        const auto telemetryValid = options.telemetry.validate();
        if (!telemetryValid.ok)
            return reportFailure("telemetry configuration", telemetryValid);
        if (static_cast<std::uint64_t>(options.database.connectTimeoutSeconds) * 1000U >
                options.telemetry.databaseOperationTimeoutMs ||
            static_cast<std::uint64_t>(options.database.lockWaitTimeoutSeconds) * 1000U >
                options.telemetry.databaseOperationTimeoutMs)
        {
            const auto invalid = SZX::COMMON::Result<void>::Failure(
                static_cast<int>(ErrorCode::InvalidConfiguration),
                "Database connect and lock wait timeouts must not exceed telemetry.databaseOperationTimeoutMs.");
            return reportFailure("database/telemetry timeout configuration", invalid);
        }
    }
    DatabasePool database(options.database, options.logs);
    auto result = database.start();
    if (!result.ok)
        return reportFailure("database startup", result);
    if (!maintenance.empty())
    {
        const auto maintenanceResult = runMaintenance(database, options.logs, maintenance, confirmPurge);
        const auto stopped           = database.stop();
        return stopped.ok ? maintenanceResult : reportFailure("database shutdown", stopped);
    }

    SZX::ADMIN::AUTH::LoginService login(database, options.auth);
    const auto authReady = login.initialize();
    if (!authReady.ok)
        return reportFailure("authentication configuration/key initialization", authReady);
    LOGM_INFO("szx_admin", "event=authentication_initialized");
    result = database.withConnection(
        [&](auto &connection)
        {
            const auto migration = MigrationRunner{}.run(connection);
            if (!migration.ok)
                return SZX::COMMON::Result<void>::Failure(migration.error, migration.message);
            const auto bootstrap = SZX::ADMIN::INFRA::BootstrapAdminService{}.run(connection);
            if (!bootstrap.ok)
                return SZX::COMMON::Result<void>::Failure(bootstrap.error, bootstrap.message);
            LOGM_INFO("szx_admin", "event=database_initialized schema_version={} migrations_applied={} bootstrap_created={}",
                      migration.value.currentVersion, migration.value.appliedCount, bootstrap.value);
            return SZX::COMMON::Result<void>::Success();
        });
    if (!result.ok)
    {
        (void)database.stop();
        return reportFailure("database initialization", result);
    }

    SZX::ADMIN::TELEMETRY::BehaviorEventWriter behaviorWriter(database, options.telemetry);
    result = behaviorWriter.start();
    if (!result.ok)
    {
        (void)database.stop();
        return reportFailure("behavior event writer startup", result);
    }

    HttpTransport transport(options.server);
    const auto routes = SZX::ADMIN::registerRoutes(transport, database, &login, &behaviorWriter);
    if (!routes.ok)
    {
        (void)behaviorWriter.stop();
        (void)database.stop();
        return reportFailure("route registration", routes);
    }

    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
#if defined(_WIN32)
    std::signal(SIGBREAK, requestWriterResume);
#else
    std::signal(SIGHUP, requestWriterResume);
#endif
    result = transport.start();
    if (!result.ok)
    {
        (void)behaviorWriter.stop();
        (void)database.stop();
        return reportFailure("HTTP startup", result);
    }

    LOGM_INFO("szx_admin", "szx_admin_service listening on {}:{}", options.server.host, transport.port());
    while (!g_stopRequested.load(std::memory_order_relaxed))
    {
        if (g_writerResumeRequested.exchange(false, std::memory_order_relaxed))
        {
            const auto resumed = behaviorWriter.resumeAfterFault();
            if (resumed.ok)
                LOGM_INFO("szx_admin.telemetry", "event=writer_resumed");
            else
                LOGM_WARN("szx_admin.telemetry", "event=writer_resume_rejected code={}", resumed.error);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{250});
    }

    LOGM_INFO("szx_admin", "event=shutdown_requested");
    const auto stopResult     = transport.stop();
    const auto drained        = behaviorWriter.stop();
    const auto databaseResult = database.stop();
    if (!stopResult.ok)
        return reportFailure("HTTP shutdown", stopResult);
    if (!databaseResult.ok)
        return reportFailure("database shutdown", databaseResult);
    if (!drained.clean)
    {
        LOGM_ERROR("szx_admin",
                   "event=behavior_writer_drain_incomplete discarded={} pending_commit_unknown={} remaining_queue={} "
                   "remaining_batch={}",
                   drained.discarded, drained.pendingCommitUnknown, drained.remainingQueued, drained.remainingBatch);
        return 1;
    }
    return 0;
}
