/**
 * @file Logger_test.cc
 * @brief Logger_test.cc 覆盖的单元测试与测试辅助。
 */

#include "Logger.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using namespace SZX::LOGGER;

namespace
{

std::filesystem::path loggerTempDir()
{
    return std::filesystem::temp_directory_path() / "aoi_pcb_logger_ut";
}

void cleanupLoggerTempDir()
{
    LogOptions options;
    options.logger_name = "LOGGER_UT_CLEANUP";
    options.file_path   = "";
    options.console     = false;
    init(options);
    flush();

    std::error_code ignored;
    std::filesystem::remove_all(loggerTempDir(), ignored);
}

class LoggerTempCleanupEnvironment final : public ::testing::Environment
{
public:
    void SetUp() override
    {
        cleanupLoggerTempDir();
    }

    void TearDown() override
    {
        cleanupLoggerTempDir();
    }
};

const auto *const kLoggerTempCleanupEnvironment = ::testing::AddGlobalTestEnvironment(new LoggerTempCleanupEnvironment);

std::filesystem::path makeLogPath(const std::string &name)
{
    const auto dir = loggerTempDir();
    std::filesystem::create_directories(dir);
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path        = dir / (name + "_" + std::to_string(stamp) + ".log");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return path;
}

LogOptions makeOptions()
{
    LogOptions options;
    options.level         = LogLevel::Info;
    options.flush_level   = LogLevel::Trace;
    options.logger_name   = "LOGGER_UT";
    options.pattern       = "%n|%l|%v";
    options.max_file_size = 1024 * 1024;
    options.max_files     = 1;
    options.truncate      = true;
    options.console       = false;
    return options;
}

std::string readText(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream stream;
    stream << input.rdbuf();
    return stream.str();
}

std::size_t countOccurrences(std::string_view text, std::string_view needle)
{
    std::size_t count  = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string_view::npos)
    {
        ++count;
        offset += needle.size();
    }
    return count;
}

} // namespace

TEST(LoggerTest, InitializesFileLoggerAndFiltersDefaultLevel)
{
    const auto logPath     = makeLogPath("default_level");
    auto options           = makeOptions();
    const auto logPathText = logPath.string();
    options.file_path      = logPathText.c_str();

    init(options);
    EXPECT_EQ(get_level(), LogLevel::Info);
    EXPECT_FALSE(should_log(LogLevel::Debug));
    EXPECT_TRUE(should_log(LogLevel::Error));

    LOG_DEBUG("debug message should be filtered");
    LOG_INFO("info message {}", 42);
    flush();

    const auto content = readText(logPath);
    EXPECT_NE(content.find("===== Application Start ====="), std::string::npos);
    EXPECT_NE(content.find("info message 42"), std::string::npos);
    EXPECT_EQ(content.find("debug message should be filtered"), std::string::npos);
}

TEST(LoggerTest, UpdatesDefaultAndModuleLevels)
{
    const auto logPath     = makeLogPath("module_level");
    auto options           = makeOptions();
    const auto logPathText = logPath.string();
    options.file_path      = logPathText.c_str();
    init(options);

    set_level(LogLevel::Warn);
    EXPECT_EQ(get_level(), LogLevel::Warn);
    EXPECT_FALSE(should_log(LogLevel::Info));
    EXPECT_TRUE(should_log(LogLevel::Critical));

    constexpr std::string_view module = "logger_ut_module";
    EXPECT_TRUE(should_log(module, LogLevel::Warn));
    set_level(module, LogLevel::Error);
    EXPECT_EQ(get_level(module), LogLevel::Error);
    EXPECT_FALSE(should_log(module, LogLevel::Warn));
    EXPECT_TRUE(should_log(module, LogLevel::Error));

    LOG_INFO("default info should be filtered");
    LOG_WARN("default warn should be written");
    LOGM_WARN(module.data(), "module warn should be filtered");
    LOGM_ERROR(module.data(), "module error should be written");
    flush();

    const auto content = readText(logPath);
    EXPECT_EQ(content.find("default info should be filtered"), std::string::npos);
    EXPECT_NE(content.find("default warn should be written"), std::string::npos);
    EXPECT_EQ(content.find("module warn should be filtered"), std::string::npos);
    EXPECT_NE(content.find("logger_ut_module|ERROR|module error should be written"), std::string::npos);
}

TEST(LoggerTest, ConditionalAndOnceMacrosRespectConditions)
{
    const auto logPath     = makeLogPath("macros");
    auto options           = makeOptions();
    const auto logPathText = logPath.string();
    options.file_path      = logPathText.c_str();
    options.level          = LogLevel::Trace;
    init(options);

    LOG_INFO_IF(false, "conditional false should be skipped");
    LOG_INFO_IF(true, "conditional true should be written");
    LOGM_WARN_IF("logger_macro_module", false, "module conditional false should be skipped");
    LOGM_WARN_IF("logger_macro_module", true, "module conditional true should be written");

    for (int i = 0; i < 3; ++i)
    {
        LOG_WARN_ONCE("default once message");
        LOGM_ERROR_ONCE("logger_macro_module", "module once message");
    }
    flush();

    const auto content = readText(logPath);
    EXPECT_EQ(content.find("conditional false should be skipped"), std::string::npos);
    EXPECT_NE(content.find("conditional true should be written"), std::string::npos);
    EXPECT_EQ(content.find("module conditional false should be skipped"), std::string::npos);
    EXPECT_NE(content.find("module conditional true should be written"), std::string::npos);

    const auto firstDefaultOnce = content.find("default once message");
    ASSERT_NE(firstDefaultOnce, std::string::npos);
    EXPECT_EQ(content.find("default once message", firstDefaultOnce + 1), std::string::npos);

    const auto firstModuleOnce = content.find("module once message");
    ASSERT_NE(firstModuleOnce, std::string::npos);
    EXPECT_EQ(content.find("module once message", firstModuleOnce + 1), std::string::npos);
}

TEST(LoggerTest, SupportsEveryRuntimeLogLevel)
{
    const auto logPath     = makeLogPath("all_levels");
    auto options           = makeOptions();
    const auto logPathText = logPath.string();
    options.file_path      = logPathText.c_str();
    options.level          = LogLevel::Trace;
    init(options);

    LOG_TRACE("trace level message");
    LOG_DEBUG("debug level message");
    LOG_INFO("info level message");
    LOG_WARN("warn level message");
    LOG_ERROR("error level message");
    LOG_CRITICAL("critical level message");

    constexpr std::string_view module = "logger_level_module";
    LOGM_TRACE(module.data(), "module trace level message");
    LOGM_DEBUG(module.data(), "module debug level message");
    LOGM_INFO(module.data(), "module info level message");
    LOGM_WARN(module.data(), "module warn level message");
    LOGM_ERROR(module.data(), "module error level message");
    LOGM_CRITICAL(module.data(), "module critical level message");

    flush();
    const auto content = readText(logPath);
    for (const auto *message :
         {"trace level message", "debug level message", "info level message", "warn level message", "error level message",
          "critical level message", "module trace level message", "module debug level message", "module info level message",
          "module warn level message", "module error level message", "module critical level message"})
    {
        EXPECT_NE(content.find(message), std::string::npos) << message;
    }

    using Level = LogLevel;
    constexpr std::array levels{Level::Trace, Level::Debug, Level::Info, Level::Warn, Level::Error, Level::Critical, Level::Off};
    for (const auto threshold : levels)
    {
        set_level(threshold);
        EXPECT_EQ(get_level(), threshold);
        for (const auto level : levels)
        {
            EXPECT_EQ(should_log(level), static_cast<int>(level) >= static_cast<int>(threshold));
        }
    }
}

TEST(LoggerTest, SupportsNoFileAndLazyInitializationPaths)
{
    LogOptions consoleOnlyOptions = makeOptions();
    consoleOnlyOptions.file_path  = "";
    consoleOnlyOptions.console    = true;
    consoleOnlyOptions.level      = LogLevel::Info;
    init(consoleOnlyOptions);
    EXPECT_NO_THROW(LOG_INFO("console-only logger path"));

    LogOptions nullFileOptions = makeOptions();
    nullFileOptions.file_path  = nullptr;
    nullFileOptions.console    = false;
    nullFileOptions.level      = LogLevel::Trace;
    init(nullFileOptions);
    EXPECT_NO_THROW(LOG_TRACE("null file path uses available sinks"));
    EXPECT_NO_THROW(flush());

    LogOptions emptyFileOptions = makeOptions();
    emptyFileOptions.file_path  = "";
    emptyFileOptions.console    = false;
    emptyFileOptions.level      = LogLevel::Trace;
    init(emptyFileOptions);
    EXPECT_NO_THROW(detail::log(LogLevel::Debug, "direct default log"));
    EXPECT_NO_THROW(detail::log("direct_module", LogLevel::Info, "direct module log"));
    EXPECT_NO_THROW(detail::log(static_cast<LogLevel>(255), "invalid level maps to info"));
    EXPECT_NO_THROW(set_level("direct_module", static_cast<LogLevel>(255)));
    EXPECT_TRUE(should_log("direct_module", LogLevel::Info));
    EXPECT_TRUE(should_log("direct_module", LogLevel::Critical));
    EXPECT_EQ(get_level("direct_module"), LogLevel::Info);
    flush();
}

TEST(LoggerTest, FlushBeforeInitializationIsNoop)
{
    EXPECT_NO_THROW(flush());
}

TEST(LoggerTest, DetailLogLazilyInitializesDefaultLogger)
{
    EXPECT_NO_THROW(detail::log(LogLevel::Info, "lazy default logger message"));
    EXPECT_NO_THROW(flush());
}

TEST(LoggerTest, HandlesConcurrentDefaultAndModuleLogging)
{
    const auto logPath     = makeLogPath("threads");
    auto options           = makeOptions();
    const auto logPathText = logPath.string();
    options.file_path      = logPathText.c_str();
    options.level          = LogLevel::Trace;
    init(options);

    constexpr int threadCount = 8;
    constexpr int perThread   = 40;
    std::atomic<bool> start{false};
    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
    {
        workers.emplace_back(
            [threadIndex, &start]
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                const std::string module = "thread_module_" + std::to_string(threadIndex % 3);
                for (int i = 0; i < perThread; ++i)
                {
                    LOG_INFO("thread default {} {}", threadIndex, i);
                    LOGM_DEBUG(module, "thread module {} {}", threadIndex, i);
                }
            });
    }

    start.store(true, std::memory_order_release);
    for (auto &worker : workers)
    {
        worker.join();
    }

    flush();
    const auto content = readText(logPath);
    EXPECT_EQ(countOccurrences(content, "thread default "), static_cast<std::size_t>(threadCount * perThread));
    EXPECT_EQ(countOccurrences(content, "thread module "), static_cast<std::size_t>(threadCount * perThread));
}
