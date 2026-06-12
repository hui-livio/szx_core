#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

#include <spdlog/common.h>

namespace spdlog {
class logger;
}

struct LogOptions
{
    std::string loggerName{"camera_api"};
    std::string logDir{"logs"};
    std::string logFile{"backend_api.log"};
    std::size_t maxFileSize{20 * 1024 * 1024};
    std::size_t maxFiles{5};
    spdlog::level::level_enum level{spdlog::level::debug};
    bool consoleEnabled{true};
};

class LogManager
{
public:
    static LogManager &instance();

    bool init(const LogOptions &options, std::string *errorMessage = nullptr);
    void shutdown();

    std::shared_ptr<spdlog::logger> logger() const;
    void flush();

    LogManager(const LogManager &) = delete;
    LogManager &operator=(const LogManager &) = delete;

private:
    LogManager() = default;
    ~LogManager() = default;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    bool initialized_{false};
};
