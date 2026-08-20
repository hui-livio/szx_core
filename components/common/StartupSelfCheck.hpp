/**
 * @file StartupSelfCheck.hpp
 * @brief 以 RAII 安全退出方式验证应用启动依赖。
 */

#pragma once

#include "FileHelper.hpp"
#include "TimeHelper.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace SZX::COMMON
{

/** @brief 检查文件系统依赖，失败时记录原因并请求应用退出。 */
class StartupSelfCheck
{
public:
    enum class DependencyType
    {
        Path,
        File,
        Directory,
    };

    struct Dependency
    {
        std::filesystem::path path;
        DependencyType type = DependencyType::Path;
        std::string description;
    };

    struct Failure
    {
        enum class Reason
        {
            Missing,
            WrongType,
            FilesystemError,
        };

        Dependency dependency;
        std::filesystem::path resolvedPath;
        Reason reason = Reason::Missing;
        std::error_code errorCode;
    };

    struct Result
    {
        enum class LogStatus
        {
            NotRequired,
            Written,
            Failed,
        };

        std::vector<Failure> failures;
        LogStatus logStatus = LogStatus::NotRequired;
        std::filesystem::path logFile;
        std::error_code logError;

        [[nodiscard]] bool passed() const noexcept
        {
            return failures.empty();
        }

        [[nodiscard]] bool logWritten() const noexcept
        {
            return logStatus == LogStatus::Written;
        }
    };

    class ExitRequested final : public std::exception
    {
    public:
        explicit ExitRequested(int exitCode) noexcept :
            exitCode_(exitCode)
        {
        }

        [[nodiscard]] int exitCode() const noexcept
        {
            return exitCode_;
        }
        [[nodiscard]] const char *what() const noexcept override
        {
            return "startup self-check requested application exit";
        }

    private:
        int exitCode_;
    };

    struct Input
    {
        enum class Termination
        {
            Exit,
            Abort,
        };

        std::filesystem::path baseDirectory;
        std::vector<Dependency> dependencies;
        std::filesystem::path failureLogFile;
        std::function<void(const Result &)> failureCallback;
        Termination termination = Termination::Exit;
        int exitCode            = EXIT_FAILURE;
    };

    /**
     * @brief 执行完整启动自检流程。
     *
     * 检查成功时正常返回；失败时依次写入日志、调用异常回调，并抛出 ExitRequested
     * 或直接 abort。ExitRequested 应在 main 边界转换为退出码，以保留 RAII 栈展开。
     */
    static void run(const Input &input)
    {
        Result result = checkAndLog(input.baseDirectory, input.dependencies, input.failureLogFile);
        if (result.passed())
            return;

        if (input.failureCallback)
        {
            try
            {
                input.failureCallback(result);
            } catch (const std::exception &exception)
            {
                std::cerr << "Startup self-check failure callback threw: " << exception.what() << '\n';
            } catch (...)
            {
                std::cerr << "Startup self-check failure callback threw an unknown exception\n";
            }
        }

        if (input.termination == Input::Termination::Abort)
            std::abort();
        throw ExitRequested(input.exitCode);
    }

    /**
     * @brief 检查启动依赖；相对路径以 baseDirectory 为基准解析。
     */
    [[nodiscard]] static Result check(const std::filesystem::path &baseDirectory, const std::vector<Dependency> &dependencies)
    {
        Result result;
        for (const auto &dependency : dependencies)
        {
            const auto resolvedPath = dependency.path.is_absolute() ? dependency.path : baseDirectory / dependency.path;

            std::error_code error;
            const auto status = std::filesystem::status(resolvedPath, error);
            if (error)
            {
                result.failures.push_back({dependency, resolvedPath.lexically_normal(), Failure::Reason::FilesystemError, error});
                return result;
            }

            if (!std::filesystem::exists(status))
            {
                result.failures.push_back({dependency, resolvedPath.lexically_normal(), Failure::Reason::Missing, {}});
                return result;
            }

            bool valid = true;
            switch (dependency.type)
            {
            case DependencyType::Path:
                break;
            case DependencyType::File:
                valid = std::filesystem::is_regular_file(status);
                break;
            case DependencyType::Directory:
                valid = std::filesystem::is_directory(status);
                break;
            }

            if (!valid)
            {
                result.failures.push_back({dependency, resolvedPath.lexically_normal(), Failure::Reason::WrongType, {}});
                return result;
            }
        }
        return result;
    }

    /**
     * @brief 按顺序检查启动依赖，遇到第一项失败时立即写入日志并停止检查。
     */
    [[nodiscard]] static Result checkAndLog(const std::filesystem::path &baseDirectory,
                                            const std::vector<Dependency> &dependencies, const std::filesystem::path &logFile)
    {
        Result result = check(baseDirectory, dependencies);
        if (result.passed())
            return result;

        static std::mutex logMutex;
        const std::lock_guard lock(logMutex);

        result.logFile = logFile;
        std::ostringstream record;
        auto timestamp = TimeHelper::TimestampToString(TimeHelper::NowTimestamp(), "%Y-%m-%d %H:%M:%S", false);
        if (timestamp.empty())
            timestamp = "timestamp-unavailable";

        record << '[' << timestamp << "] startup self-check failed: " << result.failures.size()
               << " dependency item(s) unavailable\n";
        for (const auto &failure : result.failures)
        {
            record << "  - expected=" << typeName(failure.dependency.type) << ", path=" << failure.resolvedPath.string()
                   << ", reason=" << reasonName(failure.reason);
            if (failure.errorCode)
                record << ", error=" << failure.errorCode.message() << " (" << failure.errorCode.value() << ')';
            if (!failure.dependency.description.empty())
                record << ", description=" << failure.dependency.description;
            record << '\n';
        }

        if (!FileHelper::AppendText(logFile, record.str()))
        {
            result.logStatus = Result::LogStatus::Failed;
            result.logError  = std::make_error_code(std::io_errc::stream);
            return result;
        }
        result.logStatus = Result::LogStatus::Written;
        return result;
    }

private:
    [[nodiscard]] static const char *typeName(DependencyType type) noexcept
    {
        switch (type)
        {
        case DependencyType::File:
            return "file";
        case DependencyType::Directory:
            return "directory";
        case DependencyType::Path:
            return "path";
        }
        return "unknown";
    }

    [[nodiscard]] static const char *reasonName(Failure::Reason reason) noexcept
    {
        switch (reason)
        {
        case Failure::Reason::Missing:
            return "missing";
        case Failure::Reason::WrongType:
            return "wrong_type";
        case Failure::Reason::FilesystemError:
            return "filesystem_error";
        }
        return "unknown";
    }
};

} // namespace SZX::COMMON
