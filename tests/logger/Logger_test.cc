#include <gtest/gtest.h>

#include "Logger.h"

#include <string_view>

using namespace SZX::LOGGER;

TEST(LoggerTest, InitializesAndFiltersDefaultLevel)
{
    LogOptions options;
    options.logger_name = "SZX_LOGGER_UT";
    options.file_path   = "";
    options.console     = false;
    options.level       = LogLevel::Info;
    init(options);

    EXPECT_EQ(get_level(), LogLevel::Info);
    EXPECT_FALSE(should_log(LogLevel::Debug));
    EXPECT_TRUE(should_log(LogLevel::Error));
    EXPECT_NO_THROW(LOG_INFO("logger unit test"));
    EXPECT_NO_THROW(flush());
}

TEST(LoggerTest, UpdatesDefaultAndModuleLevels)
{
    LogOptions options;
    options.logger_name = "SZX_LOGGER_UT_LEVEL";
    options.file_path   = "";
    options.console     = false;
    init(options);

    set_level(LogLevel::Warn);
    EXPECT_EQ(get_level(), LogLevel::Warn);
    EXPECT_FALSE(should_log(LogLevel::Info));
    EXPECT_TRUE(should_log(LogLevel::Critical));

    constexpr std::string_view module = "logger_ut_module";
    set_level(module, LogLevel::Error);
    EXPECT_EQ(get_level(module), LogLevel::Error);
    EXPECT_FALSE(should_log(module, LogLevel::Warn));
    EXPECT_TRUE(should_log(module, LogLevel::Error));
}
