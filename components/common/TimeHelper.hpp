/**
 * @file TimeHelper.hpp
 * @brief   时间工具类 — 提供时间戳获取、ISO 8601 格式化、时间字符串解析等功能
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace SZX::COMMON
{
// 时间工具类：提供获取当前时间戳、格式化时间、解析时间字符串等功能
class TimeHelper final
{
public:
    // 获取当前时间戳（单位：毫秒）
    static int64_t NowTimestamp()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    // 获取当前时间的 ISO 8601 格式字符串（UTC 时间）
    static std::string NowIso8601()
    {
        return TimestampToIso8601(NowTimestamp());
    }

    // 时间戳(毫秒)转换为格式化时间字符串
    static std::string TimestampToString(int64_t timestampMs, const char *format = "%Y-%m-%d %H:%M:%S", bool utc = true)
    {
        const auto timeValue = static_cast<std::time_t>(timestampMs / 1000);
        std::tm tm{};

        if (utc)
        {
            if (!ToUtcTm(timeValue, tm))
            {
                return {};
            }
        }
        else
        {
            if (!ToLocalTm(timeValue, tm))
            {
                return {};
            }
        }

        char buffer[64]{};
        if (std::strftime(buffer, sizeof(buffer), format, &tm) == 0)
        {
            return {};
        }

        return std::string(buffer);
    }

    // 按给定格式把时间字符串解析为时间戳(毫秒)
    static std::optional<int64_t> StringToTimestamp(const std::string &text, const char *format = "%Y-%m-%d %H:%M:%S",
                                                    bool utc = true)
    {
        std::tm tm{};
        std::istringstream input(text);
        input >> std::get_time(&tm, format);
        if (input.fail())
        {
            return std::nullopt;
        }
        input >> std::ws;
        if (!input.eof())
        {
            return std::nullopt;
        }

        const auto parsedTm   = tm;
        tm.tm_isdst           = -1;
        std::time_t timeValue = utc ? UtcTmToTime(tm) : std::mktime(&tm);
        if (timeValue == static_cast<std::time_t>(-1))
        {
            return std::nullopt;
        }

        std::tm normalized{};
        const bool converted = utc ? ToUtcTm(timeValue, normalized) : ToLocalTm(timeValue, normalized);
        if (!converted || !SameCalendarTime(parsedTm, normalized))
        {
            return std::nullopt;
        }

        return static_cast<int64_t>(timeValue) * 1000;
    }

    // 时间戳(毫秒)转换为 ISO 8601 UTC 字符串（YYYY-MM-DDTHH:MM:SSZ）
    static std::string TimestampToIso8601(int64_t timestampMs)
    {
        return TimestampToString(timestampMs, "%Y-%m-%dT%H:%M:%SZ", true);
    }

    // ISO 8601 UTC 字符串（YYYY-MM-DDTHH:MM:SSZ）解析为时间戳(毫秒)
    static std::optional<int64_t> Iso8601ToTimestamp(std::string_view iso8601)
    {
        if (iso8601.size() != 20 || iso8601.back() != 'Z')
        {
            return std::nullopt;
        }
        return StringToTimestamp(std::string(iso8601), "%Y-%m-%dT%H:%M:%SZ", true);
    }

private:
    static bool ToUtcTm(std::time_t timeValue, std::tm &out)
    {
#if defined(_WIN32)
        return gmtime_s(&out, &timeValue) == 0;
#else
        return gmtime_r(&timeValue, &out) != nullptr;
#endif
    }

    static bool ToLocalTm(std::time_t timeValue, std::tm &out)
    {
#if defined(_WIN32)
        return localtime_s(&out, &timeValue) == 0;
#else
        return localtime_r(&timeValue, &out) != nullptr;
#endif
    }

    static std::time_t UtcTmToTime(std::tm &tm)
    {
#if defined(_WIN32)
        return _mkgmtime(&tm);
#else
        return timegm(&tm);
#endif
    }

    static bool SameCalendarTime(const std::tm &lhs, const std::tm &rhs)
    {
        return lhs.tm_year == rhs.tm_year && lhs.tm_mon == rhs.tm_mon && lhs.tm_mday == rhs.tm_mday &&
            lhs.tm_hour == rhs.tm_hour && lhs.tm_min == rhs.tm_min && lhs.tm_sec == rhs.tm_sec;
    }
};

// 简单秒表：用于手动控制起止并读取耗时
class Stopwatch final
{
public:
    using Clock = std::chrono::steady_clock;

    Stopwatch() :
        start_(Clock::now())
    {
    }

    void Reset()
    {
        start_ = Clock::now();
    }

    [[nodiscard]] int64_t ElapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count();
    }

private:
    Clock::time_point start_;
};

// 作用域计时器：析构时回调耗时毫秒，可提前 Stop()
class ScopeTimer final
{
public:
    using Clock = std::chrono::steady_clock;

    explicit ScopeTimer(std::function<void(int64_t)> onElapsedMs = {}) :
        start_(Clock::now()),
        callback_(std::move(onElapsedMs))
    {
    }

    ~ScopeTimer()
    {
        Stop();
    }

    ScopeTimer(const ScopeTimer &)            = delete;
    ScopeTimer &operator=(const ScopeTimer &) = delete;

    ScopeTimer(ScopeTimer &&other) noexcept :
        start_(other.start_),
        callback_(std::move(other.callback_)),
        stopped_(other.stopped_)
    {
        other.stopped_ = true;
    }

    ScopeTimer &operator=(ScopeTimer &&other) noexcept
    {
        if (this != &other)
        {
            Stop();
            start_         = other.start_;
            callback_      = std::move(other.callback_);
            stopped_       = other.stopped_;
            other.stopped_ = true;
        }
        return *this;
    }

    [[nodiscard]] int64_t ElapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_).count();
    }

    void Stop()
    {
        if (stopped_)
        {
            return;
        }

        stopped_ = true;
        if (callback_)
        {
            callback_(ElapsedMs());
        }
    }

private:
    Clock::time_point start_;
    std::function<void(int64_t)> callback_;
    bool stopped_ = false;
};
} // namespace SZX::COMMON
