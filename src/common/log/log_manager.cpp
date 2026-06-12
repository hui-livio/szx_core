#include "common/log/log_manager.h"

#include <filesystem>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace {
constexpr const char *kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid:%t] [%n] %v";
}

LogManager &LogManager::instance()
{
    static LogManager instance;
    return instance;
}

bool LogManager::init(const LogOptions &options, std::string *errorMessage)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return true;
    }

    try {
        std::filesystem::create_directories(options.logDir);
        spdlog::drop(options.loggerName);

        std::vector<spdlog::sink_ptr> sinks;
        if (options.consoleEnabled) {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern(kPattern);
            sinks.push_back(consoleSink);
        }

        auto logPath = (std::filesystem::path(options.logDir) / options.logFile).string();
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logPath, options.maxFileSize, options.maxFiles, true);
        fileSink->set_pattern(kPattern);
        sinks.push_back(fileSink);

        logger_ = std::make_shared<spdlog::logger>(options.loggerName, sinks.begin(), sinks.end());
        logger_->set_level(options.level);
        logger_->flush_on(spdlog::level::err);

        spdlog::set_default_logger(logger_);
        initialized_ = true;
        logger_->info("logger initialized, output file={}", logPath);
        return true;
    } catch (const std::exception &ex) {
        if (errorMessage) {
            *errorMessage = ex.what();
        }
        return false;
    }
}

void LogManager::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return;
    }

    if (logger_) {
        logger_->info("logger shutting down");
        logger_->flush();
    }

    spdlog::shutdown();
    logger_.reset();
    initialized_ = false;
}

std::shared_ptr<spdlog::logger> LogManager::logger() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_) {
        return logger_;
    }
    return spdlog::default_logger();
}

void LogManager::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_) {
        logger_->flush();
    }
}
