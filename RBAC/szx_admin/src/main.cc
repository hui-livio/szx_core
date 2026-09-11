#include "Command.hpp"
#include "FileHelper.hpp"
#include "Logger.h"
#include "Serializer.hpp"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/ServerConfig.h"

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
using SZX::ADMIN::INFRA::HttpResponse;
using SZX::ADMIN::INFRA::HttpTransport;
using SZX::ADMIN::INFRA::MigrationRunner;
using SZX::ADMIN::INFRA::ServerConfig;

std::atomic_bool g_stopRequested{};

void requestStop(int) noexcept
{
    g_stopRequested.store(true, std::memory_order_relaxed);
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
};
SZX_REFL(ServiceConfig::Logging, level, flushLevel, loggerName, pattern, filePath, maxFileSize, maxFiles, truncate, console);
SZX_REFL(ServiceConfig, logging, server, database);

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

auto loadConfig(int argc, char **argv, ServiceConfig &config) -> int
{
    SZX::COMMON::Command command{"SZX Admin service", "SZX Admin service"};
    std::string configArgument;
    command.add_option("-c,--config", configArgument, "YAML configuration file path");
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
    const auto content      = SZX::COMMON::FileHelper::ReadText(resolvedPath);
    if (!content)
    {
        std::cerr << "cannot read configuration file: " << resolvedPath.string() << '\n';
        return 2;
    }

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
} // namespace

int main(int argc, char **argv)
{
    ServiceConfig options;
    const auto configResult = loadConfig(argc, argv, options);
    if (configResult != 0)
        return configResult < 0 ? 0 : configResult;

    const auto serverValid = options.server.validate();
    if (!serverValid.ok)
        return reportFailure("server configuration", serverValid);
    const auto databaseValid = options.database.validate();
    if (!databaseValid.ok)
        return reportFailure("database configuration", databaseValid);

    DatabasePool database(options.database);
    auto result = database.start();
    if (!result.ok)
        return reportFailure("database startup", result);

    result = database.withConnection(
        [&](auto &connection)
        {
            const auto migration = MigrationRunner{}.run(connection);
            if (!migration.ok)
                return SZX::COMMON::Result<void>::Failure(migration.error, migration.message);
            const auto transaction = connection.begin();
            if (!transaction.ok) return transaction;
            const auto bootstrap = SZX::ADMIN::INFRA::BootstrapAdminService{}.run(connection);
            if (!bootstrap.ok) { (void)connection.rollback(); return SZX::COMMON::Result<void>::Failure(bootstrap.error, bootstrap.message); }
            return connection.commit();
        });
    if (!result.ok)
    {
        (void)database.stop();
        return reportFailure("database initialization", result);
    }

    HttpTransport transport(options.server);
    const auto readyPath  = options.server.apiPrefix == "/" ? "/ready" : options.server.apiPrefix + "/ready";
    const auto readyRoute = transport.registerRoute(
        "GET", readyPath,
        [&database](const auto &)
        {
            const auto ping = database.ping();
            if (!ping.ok)
                return HttpResponse{
                    .statusCode = 503, .body = "{\"status\":\"unavailable\"}", .headers = {{"Content-Type", "application/json"}}};
            return HttpResponse{
                .statusCode = 200, .body = "{\"status\":\"ready\"}", .headers = {{"Content-Type", "application/json"}}};
        });
    if (!readyRoute.ok)
    {
        (void)database.stop();
        return reportFailure("ready route registration", readyRoute);
    }

    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    result = transport.start();
    if (!result.ok)
    {
        (void)database.stop();
        return reportFailure("HTTP startup", result);
    }

    LOGM_INFO("szx_admin", "szx_admin_service listening on {}:{}", options.server.host, transport.port());
    while (!g_stopRequested.load(std::memory_order_relaxed))
        std::this_thread::sleep_for(std::chrono::milliseconds{250});

    const auto stopResult     = transport.stop();
    const auto databaseResult = database.stop();
    if (!stopResult.ok)
        return reportFailure("HTTP shutdown", stopResult);
    if (!databaseResult.ok)
        return reportFailure("database shutdown", databaseResult);
    return 0;
}
