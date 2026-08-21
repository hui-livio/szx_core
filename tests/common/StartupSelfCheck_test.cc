/**
 * @file StartupSelfCheck_test.cc
 * @brief StartupSelfCheck_test.cc 覆盖的单元测试与测试辅助。
 */

#include "StartupSelfCheck.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace SZX::COMMON;

namespace
{

class StartupSelfCheckTest : public testing::Test
{
protected:
    void SetUp() override
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        root_             = std::filesystem::temp_directory_path() / ("aoi_pcb_self_check_" + std::to_string(suffix));
        std::filesystem::create_directories(root_ / "config");
        std::ofstream(root_ / "config" / "app.yaml") << "ready: true\n";
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    std::filesystem::path root_;
};

void runFailingSelfCheckForAbort()
{
    using SelfCheck = StartupSelfCheck;
#if defined(_WIN32)
    const auto nullDevice = std::filesystem::path{"NUL"};
#else
    const auto nullDevice = std::filesystem::path{"/dev/null"};
#endif
    const SelfCheck::Input input{
        .baseDirectory  = std::filesystem::current_path(),
        .dependencies   = {{"missing.file", SelfCheck::DependencyType::File, "required file"}},
        .failureLogFile = nullDevice,
        .failureCallback =
            [](const SelfCheck::Result &)
        {
            std::fprintf(stderr, "abort-callback\n");
            std::fflush(stderr);
        },
        .termination = SelfCheck::Input::Termination::Abort,
    };
    SelfCheck::run(input);
}

TEST_F(StartupSelfCheckTest, PassesWhenRequiredFileAndDirectoryExist)
{
    using SelfCheck   = StartupSelfCheck;
    const auto result = SelfCheck::check(
        root_, {{"config", SelfCheck::DependencyType::Directory, {}}, {"config/app.yaml", SelfCheck::DependencyType::File, {}}});

    EXPECT_TRUE(result.passed());
}

TEST_F(StartupSelfCheckTest, StopsAtAndLogsFirstMissingDependency)
{
    using SelfCheck    = StartupSelfCheck;
    const auto logFile = root_ / "logs" / "run_exception.logh";
    const auto result  = SelfCheck::checkAndLog(root_,
                                                {{"missing-dir", SelfCheck::DependencyType::Directory, "models"},
                                                 {"config/missing.yaml", SelfCheck::DependencyType::File, "configuration"}},
                                                logFile);

    ASSERT_FALSE(result.passed());
    ASSERT_EQ(result.failures.size(), 1U);
    EXPECT_TRUE(result.logWritten());
    ASSERT_TRUE(std::filesystem::is_regular_file(logFile));

    std::ifstream stream(logFile);
    const std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("startup self-check failed"), std::string::npos);
    EXPECT_NE(content.find("missing-dir"), std::string::npos);
    EXPECT_EQ(content.find("missing.yaml"), std::string::npos);
}

TEST_F(StartupSelfCheckTest, RejectsDirectoryWhenFileIsRequired)
{
    using SelfCheck   = StartupSelfCheck;
    const auto result = SelfCheck::check(root_, {{"config", SelfCheck::DependencyType::File, "must be a regular file"}});

    ASSERT_FALSE(result.passed());
    EXPECT_EQ(result.failures.size(), 1U);
    EXPECT_EQ(result.failures.front().reason, SelfCheck::Failure::Reason::WrongType);
}

TEST_F(StartupSelfCheckTest, ExitsAfterInvokingFailureCallback)
{
    using SelfCheck      = StartupSelfCheck;
    bool callbackInvoked = false;
    const SelfCheck::Input input{
        .baseDirectory   = root_,
        .dependencies    = {{"missing.file", SelfCheck::DependencyType::File, "required file"}},
        .failureLogFile  = root_ / "logs" / "run_exception.logh",
        .failureCallback = [&](const SelfCheck::Result &) { callbackInvoked = true; },
        .termination     = SelfCheck::Input::Termination::Exit,
        .exitCode        = 23,
    };

    try
    {
        SelfCheck::run(input);
        FAIL() << "Expected ExitRequested";
    } catch (const SelfCheck::ExitRequested &exit)
    {
        EXPECT_EQ(exit.exitCode(), 23);
    }
    EXPECT_TRUE(callbackInvoked);
}

TEST_F(StartupSelfCheckTest, ReturnsNormallyWithoutInvokingCallbackWhenCheckPasses)
{
    using SelfCheck      = StartupSelfCheck;
    bool callbackInvoked = false;
    const SelfCheck::Input input{
        .baseDirectory   = root_,
        .dependencies    = {{"config/app.yaml", SelfCheck::DependencyType::File, "configuration"}},
        .failureLogFile  = root_ / "logs" / "run_exception.logh",
        .failureCallback = [&](const SelfCheck::Result &) { callbackInvoked = true; },
    };

    SelfCheck::run(input);

    EXPECT_FALSE(callbackInvoked);
}

TEST(StartupSelfCheckDeathTest, AbortsAfterInvokingFailureCallback)
{
    EXPECT_DEATH(runFailingSelfCheckForAbort(), "abort-callback");
}

TEST_F(StartupSelfCheckTest, StillRequestsExitWhenFailureCallbackThrows)
{
    using SelfCheck = StartupSelfCheck;
    const SelfCheck::Input input{
        .baseDirectory   = root_,
        .dependencies    = {{"missing.file", SelfCheck::DependencyType::File, "required file"}},
        .failureLogFile  = root_ / "logs" / "run_exception.logh",
        .failureCallback = [](const SelfCheck::Result &) { throw std::runtime_error("callback failed"); },
        .termination     = SelfCheck::Input::Termination::Exit,
        .exitCode        = 31,
    };

    try
    {
        SelfCheck::run(input);
        FAIL() << "Expected ExitRequested";
    } catch (const SelfCheck::ExitRequested &exit)
    {
        EXPECT_EQ(exit.exitCode(), 31);
    }
}

TEST_F(StartupSelfCheckTest, ReportsLogWriteFailure)
{
    using SelfCheck = StartupSelfCheck;
    const auto result =
        SelfCheck::checkAndLog(root_, {{"missing.file", SelfCheck::DependencyType::File, "required file"}}, root_ / "config");

    ASSERT_FALSE(result.passed());
    EXPECT_EQ(result.logStatus, SelfCheck::Result::LogStatus::Failed);
    EXPECT_TRUE(result.logError);
}

} // namespace
