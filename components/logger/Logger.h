/**
 * @file Logger.h
 * @brief   日志系统接口 — 封装 spdlog 提供多级别、模块化日志抽象
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

// logger 模块仅支持动态库，不允许静态库和源码编译
#if defined(_WIN32) || defined(WIN64)
    #ifdef SZX_LOG_EXPORTS
        #define SZX_LOG_API __declspec(dllexport)
    #else
        #define SZX_LOG_API __declspec(dllimport)
    #endif
#else
    #define SZX_LOG_API __attribute__((visibility("default")))
#endif


namespace SZX::LOGGER
{

// 日志级别枚举
enum class LogLevel : uint8_t
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

// 日志选项
struct LogOptions
{
    LogLevel level          = LogLevel::Info; // 默认日志级别
    LogLevel flush_level    = LogLevel::Warn; // 刷新日志级别，低于该级别不刷新
    const char *logger_name = "DEFAULT";      // 日志名

    // 日志格式: 时间 tid pid 日志级别 日志模块 日志消息
    const char *pattern       = "[%Y-%m-%d %H:%M:%S.%e] [%5t|%5P] [%^%-5l%$] [%n] %v";
    const char *file_path     = "";               // 空则不写文件
    std::size_t max_file_size = 10 * 1024 * 1024; // 单文件最大字节
    std::size_t max_files     = 3;                // 保留文件数
    bool truncate             = false;            // 是否截断文件
    bool console              = true;             // 控制台输出开关：仅在 Release 下生效；Debug 模式始终输出到控制台
};

// 初始化默认日志系统（可选；首次使用时也会以默认参数自动初始化）
SZX_LOG_API void init(const LogOptions &options = {});

// 设置默认日志级别
SZX_LOG_API void set_level(LogLevel level);

// 设置指定模块日志级别
SZX_LOG_API void set_level(std::string_view mod, LogLevel level);

// 刷新所有日志
SZX_LOG_API void flush();

// 查询默认 logger 当前级别
SZX_LOG_API LogLevel get_level() noexcept;

// 查询指定模块 logger 当前级别
SZX_LOG_API LogLevel get_level(std::string_view mod);

// 默认 logger 是否会记录该级别（用于宏内提前短路，避免无谓格式化开销）
SZX_LOG_API bool should_log(LogLevel level) noexcept;

// 指定模块 logger 是否会记录该级别
SZX_LOG_API bool should_log(std::string_view mod, LogLevel level);

// ------------------------------------------------------------
// 内部接收点：接收已格式化好的字符串。
// 这些函数不是模板，spdlog 完全隐藏在实现文件中。
// ------------------------------------------------------------
namespace detail
{

// 源码位置（由日志宏通过 __FILE__/__LINE__ 填充，等价于 std::source_location）
struct SourceLoc
{
    const char *file = "";
    int line         = 0;
    const char *func = "";
};

SZX_LOG_API void log(LogLevel level, std::string_view msg, const SourceLoc &loc = {});
SZX_LOG_API void log(std::string_view mod, LogLevel level, std::string_view msg, const SourceLoc &loc = {});

} // namespace detail

// ------------------------------------------------------------
// 默认 logger 的格式化日志接口
// ------------------------------------------------------------
template <typename... Args>
void log(LogLevel level, std::format_string<Args...> fmt, Args &&...args)
{
    if (!should_log(level))
    {
        return;
    }
    detail::log(level, std::format(fmt, std::forward<Args>(args)...));
}

// 带源码位置的默认 logger 接口（供日志宏使用）
template <typename... Args>
void log_at(LogLevel level, const detail::SourceLoc &loc, std::format_string<Args...> fmt, Args &&...args)
{
    if (!should_log(level))
    {
        return;
    }
    detail::log(level, std::format(fmt, std::forward<Args>(args)...), loc);
}

// ------------------------------------------------------------
// 模块 logger 的格式化日志接口
// ------------------------------------------------------------
template <typename... Args>
void log_module(std::string_view mod, LogLevel level, std::format_string<Args...> fmt, Args &&...args)
{
    if (!should_log(mod, level))
    {
        return;
    }
    detail::log(mod, level, std::format(fmt, std::forward<Args>(args)...));
}

// 带源码位置的模块 logger 接口（供日志宏使用）
template <typename... Args>
void log_module_at(std::string_view mod, LogLevel level, const detail::SourceLoc &loc, std::format_string<Args...> fmt,
                   Args &&...args)
{
    if (!should_log(mod, level))
    {
        return;
    }
    detail::log(mod, level, std::format(fmt, std::forward<Args>(args)...), loc);
}

} // namespace SZX::LOGGER

// NOLINTBEGIN(misc-unused-macros)

// ============================================================
// 编译期级别裁剪
//   定义 SZX_INFRA_LOG_ACTIVE_LEVEL 可在编译期裁剪低于该级别的日志，
//   被裁剪的日志宏不会产生任何运行时代码。
//   例：-DSZX_INFRA_LOG_ACTIVE_LEVEL=SZX_INFRA_LOG_LEVEL_INFO
// ============================================================
#define SZX_INFRA_LOG_LEVEL_TRACE 0
#define SZX_INFRA_LOG_LEVEL_DEBUG 1
#define SZX_INFRA_LOG_LEVEL_INFO 2
#define SZX_INFRA_LOG_LEVEL_WARN 3
#define SZX_INFRA_LOG_LEVEL_ERROR 4
#define SZX_INFRA_LOG_LEVEL_CRITICAL 5
#define SZX_INFRA_LOG_LEVEL_OFF 6

#ifndef SZX_INFRA_LOG_ACTIVE_LEVEL
    #define SZX_INFRA_LOG_ACTIVE_LEVEL SZX_INFRA_LOG_LEVEL_TRACE
#endif

// 源码位置宏：__func__ 在调用处的所在函数内有效
#if defined(_MSC_VER)
    #define SZX_INFRA_LOG_FUNC __FUNCTION__
#else
    #define SZX_INFRA_LOG_FUNC __func__
#endif

#define SZX_INFRA_LOG_SRC_LOC                                                                                                    \
    ::SZX::LOGGER::detail::SourceLoc                                                                                             \
    {                                                                                                                            \
        __FILE__, __LINE__, SZX_INFRA_LOG_FUNC                                                                                   \
    }

// 内部分发宏：编译期级别裁剪 + 运行时级别短路
#define SZX_INFRA_LOG_IMPL(LEVEL_NUM, LEVEL_ENUM, ...)                                                                           \
    do                                                                                                                           \
    {                                                                                                                            \
        if constexpr ((LEVEL_NUM) >= SZX_INFRA_LOG_ACTIVE_LEVEL)                                                                 \
        {                                                                                                                        \
            ::SZX::LOGGER::log_at(::SZX::LOGGER::LogLevel::LEVEL_ENUM, SZX_INFRA_LOG_SRC_LOC, __VA_ARGS__);                      \
        }                                                                                                                        \
    } while (0)

#define SZX_INFRA_LOGM_IMPL(LEVEL_NUM, LEVEL_ENUM, module, ...)                                                                  \
    do                                                                                                                           \
    {                                                                                                                            \
        if constexpr ((LEVEL_NUM) >= SZX_INFRA_LOG_ACTIVE_LEVEL)                                                                 \
        {                                                                                                                        \
            ::SZX::LOGGER::log_module_at((module), ::SZX::LOGGER::LogLevel::LEVEL_ENUM, SZX_INFRA_LOG_SRC_LOC, __VA_ARGS__);     \
        }                                                                                                                        \
    } while (0)

// ============================================================
// 默认 logger 日志宏
//   LOG_INFO("hello {}", name);
// ============================================================
#define LOG_TRACE(...) SZX_INFRA_LOG_IMPL(SZX_INFRA_LOG_LEVEL_TRACE, Trace, __VA_ARGS__)
#define LOG_DEBUG(...) SZX_INFRA_LOG_IMPL(SZX_INFRA_LOG_LEVEL_DEBUG, Debug, __VA_ARGS__)
#define LOG_INFO(...) SZX_INFRA_LOG_IMPL(SZX_INFRA_LOG_LEVEL_INFO, Info, __VA_ARGS__)
#define LOG_WARN(...) SZX_INFRA_LOG_IMPL(SZX_INFRA_LOG_LEVEL_WARN, Warn, __VA_ARGS__)
#define LOG_ERROR(...) SZX_INFRA_LOG_IMPL(SZX_INFRA_LOG_LEVEL_ERROR, Error, __VA_ARGS__)
#define LOG_CRITICAL(...) SZX_INFRA_LOG_IMPL(SZX_INFRA_LOG_LEVEL_CRITICAL, Critical, __VA_ARGS__)

// 带 SZX 前缀的公开别名，用于新代码和文档示例。
#define SZX_LOG_TRACE(...) LOG_TRACE(__VA_ARGS__)
#define SZX_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define SZX_LOG_INFO(...) LOG_INFO(__VA_ARGS__)
#define SZX_LOG_WARN(...) LOG_WARN(__VA_ARGS__)
#define SZX_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)
#define SZX_LOG_CRITICAL(...) LOG_CRITICAL(__VA_ARGS__)

// ============================================================
// 模块 logger 日志宏
//   LOGM_DEBUG("module A", "{} {}", 1, 2);
// ============================================================
#define LOGM_TRACE(module, ...) SZX_INFRA_LOGM_IMPL(SZX_INFRA_LOG_LEVEL_TRACE, Trace, module, __VA_ARGS__)
#define LOGM_DEBUG(module, ...) SZX_INFRA_LOGM_IMPL(SZX_INFRA_LOG_LEVEL_DEBUG, Debug, module, __VA_ARGS__)
#define LOGM_INFO(module, ...) SZX_INFRA_LOGM_IMPL(SZX_INFRA_LOG_LEVEL_INFO, Info, module, __VA_ARGS__)
#define LOGM_WARN(module, ...) SZX_INFRA_LOGM_IMPL(SZX_INFRA_LOG_LEVEL_WARN, Warn, module, __VA_ARGS__)
#define LOGM_ERROR(module, ...) SZX_INFRA_LOGM_IMPL(SZX_INFRA_LOG_LEVEL_ERROR, Error, module, __VA_ARGS__)
#define LOGM_CRITICAL(module, ...) SZX_INFRA_LOGM_IMPL(SZX_INFRA_LOG_LEVEL_CRITICAL, Critical, module, __VA_ARGS__)

// ============================================================
// 条件日志：仅当 cond 为真时记录
//   LOG_INFO_IF(retry > 3, "retried {} times", retry);
// ============================================================
#define LOG_IF(LEVEL, cond, ...)                                                                                                 \
    do                                                                                                                           \
    {                                                                                                                            \
        if (cond)                                                                                                                \
        {                                                                                                                        \
            LOG_##LEVEL(__VA_ARGS__);                                                                                            \
        }                                                                                                                        \
    } while (0)

#define LOG_TRACE_IF(cond, ...) LOG_IF(TRACE, cond, __VA_ARGS__)
#define LOG_DEBUG_IF(cond, ...) LOG_IF(DEBUG, cond, __VA_ARGS__)
#define LOG_INFO_IF(cond, ...) LOG_IF(INFO, cond, __VA_ARGS__)
#define LOG_WARN_IF(cond, ...) LOG_IF(WARN, cond, __VA_ARGS__)
#define LOG_ERROR_IF(cond, ...) LOG_IF(ERROR, cond, __VA_ARGS__)
#define LOG_CRITICAL_IF(cond, ...) LOG_IF(CRITICAL, cond, __VA_ARGS__)

#define LOGM_IF(LEVEL, module, cond, ...)                                                                                        \
    do                                                                                                                           \
    {                                                                                                                            \
        if (cond)                                                                                                                \
        {                                                                                                                        \
            LOGM_##LEVEL(module, __VA_ARGS__);                                                                                   \
        }                                                                                                                        \
    } while (0)

#define LOGM_TRACE_IF(module, cond, ...) LOGM_IF(TRACE, module, cond, __VA_ARGS__)
#define LOGM_DEBUG_IF(module, cond, ...) LOGM_IF(DEBUG, module, cond, __VA_ARGS__)
#define LOGM_INFO_IF(module, cond, ...) LOGM_IF(INFO, module, cond, __VA_ARGS__)
#define LOGM_WARN_IF(module, cond, ...) LOGM_IF(WARN, module, cond, __VA_ARGS__)
#define LOGM_ERROR_IF(module, cond, ...) LOGM_IF(ERROR, module, cond, __VA_ARGS__)
#define LOGM_CRITICAL_IF(module, cond, ...) LOGM_IF(CRITICAL, module, cond, __VA_ARGS__)

// ============================================================
// 单次日志：整个程序运行期间只记录一次（线程安全，基于 magic static）
//   LOG_WARN_ONCE("deprecated API called");
// ============================================================
#define SZX_INFRA_LOG_ONCE(expr)                                                                                                 \
    do                                                                                                                           \
    {                                                                                                                            \
        static const bool _infra_logged_once = [&]() -> bool                                                                     \
        {                                                                                                                        \
            expr;                                                                                                                \
            return true;                                                                                                         \
        }();                                                                                                                     \
        (void)_infra_logged_once;                                                                                                \
    } while (0)

#define LOG_TRACE_ONCE(...) SZX_INFRA_LOG_ONCE(LOG_TRACE(__VA_ARGS__))
#define LOG_DEBUG_ONCE(...) SZX_INFRA_LOG_ONCE(LOG_DEBUG(__VA_ARGS__))
#define LOG_INFO_ONCE(...) SZX_INFRA_LOG_ONCE(LOG_INFO(__VA_ARGS__))
#define LOG_WARN_ONCE(...) SZX_INFRA_LOG_ONCE(LOG_WARN(__VA_ARGS__))
#define LOG_ERROR_ONCE(...) SZX_INFRA_LOG_ONCE(LOG_ERROR(__VA_ARGS__))
#define LOG_CRITICAL_ONCE(...) SZX_INFRA_LOG_ONCE(LOG_CRITICAL(__VA_ARGS__))

#define LOGM_TRACE_ONCE(module, ...) SZX_INFRA_LOG_ONCE(LOGM_TRACE(module, __VA_ARGS__))
#define LOGM_DEBUG_ONCE(module, ...) SZX_INFRA_LOG_ONCE(LOGM_DEBUG(module, __VA_ARGS__))
#define LOGM_INFO_ONCE(module, ...) SZX_INFRA_LOG_ONCE(LOGM_INFO(module, __VA_ARGS__))
#define LOGM_WARN_ONCE(module, ...) SZX_INFRA_LOG_ONCE(LOGM_WARN(module, __VA_ARGS__))
#define LOGM_ERROR_ONCE(module, ...) SZX_INFRA_LOG_ONCE(LOGM_ERROR(module, __VA_ARGS__))
#define LOGM_CRITICAL_ONCE(module, ...) SZX_INFRA_LOG_ONCE(LOGM_CRITICAL(module, __VA_ARGS__))
// NOLINTEND(misc-unused-macros)
