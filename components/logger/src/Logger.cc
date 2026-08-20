/**
 * @file Logger.cc
 * @brief   日志系统实现 — 封装 spdlog 提供统一的日志接口
 * @details 支持多级别日志、文件轮转、模块化配置
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#include "Logger.h"

#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace SZX::LOGGER
{

namespace
{

/**
 * @brief   转换 LogLevel 枚举到 spdlog::level
 * @param   level 项目日志级别枚举
 * @return  对应的 spdlog 日志级别
 */
spdlog::level::level_enum to_spdlog_level(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Critical:
        return spdlog::level::critical;
    case LogLevel::Off:
        return spdlog::level::off;
    }

    return spdlog::level::info;
}

/**
 * @brief   根据配置选项创建日志输出目标（sinks）
 * @param   options 日志配置选项
 * @return  sink 指针列表（控制台 + 可选文件）
 */
std::vector<spdlog::sink_ptr> create_sinks(const LogOptions &options)
{
    std::vector<spdlog::sink_ptr> sinks;

    // 控制台 sink：Debug 模式始终添加；Release 模式由 options.console 控制
#if defined(NDEBUG)
    const bool enable_console = options.console;
#else
    const bool enable_console = true;
#endif
    if (enable_console)
    {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    // 文件 sink（如果指定）
    if (options.file_path != nullptr && options.file_path[0] != '\0')
    {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(options.file_path, options.max_file_size,
                                                                               options.max_files, options.truncate));
    }

    return sinks;
}

// ========================================================================
// 全局状态：默认 logger、配置与模块 logger 注册表
// ========================================================================
struct Registry
{
    std::mutex mutex;
    LogOptions options;
    std::shared_ptr<spdlog::logger> default_logger;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> modules;

    // 共享 sink：所有 logger 复用同一组 sink，避免各自持有独立文件 sink 导致输出乱序
    std::vector<spdlog::sink_ptr> shared_sinks;

    // 热路径缓存：LogLevel 枚举本身按严重程度递增，可直接比较，无需加锁
    std::atomic<int> default_level{static_cast<int>(LogLevel::Info)};
    std::atomic<std::shared_ptr<spdlog::logger>> default_cache;
};

/**
 * @brief   获取全局注册表单例实例
 * @return  全局 Registry 引用
 */
static inline Registry &registry()
{
    static Registry instance;
    return instance;
}

/**
 * @brief   确保共享 sink 已创建（按需懒初始化）
 * @param   reg 全局注册表引用
 * @return  共享 sink 列表引用
 */
std::vector<spdlog::sink_ptr> &ensure_sinks(Registry &reg)
{
    if (reg.shared_sinks.empty())
    {
        reg.shared_sinks = create_sinks(reg.options);
    }
    return reg.shared_sinks;
}

/**
 * @brief   使用共享 sink 创建配置好的 spdlog logger 实例
 * @param   name logger 名称
 * @param   sinks 共享 sink 列表
 * @param   options 日志配置选项
 * @return  配置完成的 spdlog::logger 共享指针
 */
std::shared_ptr<spdlog::logger> make_logger(const std::string &name, const std::vector<spdlog::sink_ptr> &sinks,
                                            const LogOptions &options)
{
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(to_spdlog_level(options.level));
    logger->set_pattern(options.pattern);
    logger->flush_on(to_spdlog_level(options.flush_level));
    return logger;
}

/**
 * @brief   确保默认 logger 已初始化（按需创建）
 * @param   reg 全局注册表引用
 * @return  默认 logger 共享指针
 */
std::shared_ptr<spdlog::logger> ensure_default(Registry &reg)
{
    if (!reg.default_logger)
    {
        auto &sinks        = ensure_sinks(reg);
        reg.default_logger = make_logger(reg.options.logger_name, sinks, reg.options);
    }
    return reg.default_logger;
}

/**
 * @brief   获取或创建模块级 logger（不存在则创建）
 * @param   reg 全局注册表引用
 * @param   mod 模块名称
 * @return  模块 logger 共享指针
 */
std::shared_ptr<spdlog::logger> ensure_module(Registry &reg, std::string_view mod)
{
    std::string key(mod);
    if (auto it = reg.modules.find(key); it != reg.modules.end())
    {
        return it->second;
    }
    auto &sinks = ensure_sinks(reg);
    auto logger = make_logger(key, sinks, reg.options);
    reg.modules.emplace(key, logger);
    return logger;
}

} // anonymous namespace

/**
 * @brief   初始化日志系统并设置全局配置
 * @param   options 日志配置选项（文件路径、级别、格式等）
 */
void init(const LogOptions &options)
{
    auto &reg = registry();
    std::lock_guard lock(reg.mutex);
    reg.options = options;
    // 重新创建共享 sink（配置可能已变更）
    reg.modules.clear();
    reg.shared_sinks   = create_sinks(options);
    auto &sinks        = reg.shared_sinks;
    reg.default_logger = make_logger(options.logger_name, sinks, options);
    reg.default_level.store(static_cast<int>(options.level), std::memory_order_relaxed);
    reg.default_cache.store(reg.default_logger, std::memory_order_release);

    // 注册程序退出回调（仅首次），自动输出停止信息
    static std::once_flag atexitOnce;
    std::call_once(atexitOnce,
                   []
                   {
                       std::atexit(
                           []
                           {
                               auto &r = registry();
                               if (auto logger = r.default_cache.load(std::memory_order_acquire))
                               {
                                   logger->info("===== Application Stop =====");
                                   logger->flush();
                               }
                           });
                   });

    // 自动输出 App 启动信息，方便在日志中快速定位启动点
    reg.default_logger->info("===== Application Start =====");
}

/**
 * @brief   设置默认日志级别
 * @param   level 新的日志级别
 */
void set_level(LogLevel level)
{
    auto &reg = registry();
    std::lock_guard lock(reg.mutex);
    reg.options.level = level;
    reg.default_level.store(static_cast<int>(level), std::memory_order_relaxed);
    ensure_default(reg)->set_level(to_spdlog_level(level));
}

/**
 * @brief   设置指定模块的日志级别
 * @param   mod 模块名称
 * @param   level 新的日志级别
 */
void set_level(std::string_view mod, LogLevel level)
{
    auto &reg = registry();
    std::lock_guard lock(reg.mutex);
    ensure_module(reg, mod)->set_level(to_spdlog_level(level));
}

/**
 * @brief   获取当前默认日志级别（无锁，线程安全）
 * @return  当前默认日志级别
 */
LogLevel get_level() noexcept
{
    return static_cast<LogLevel>(registry().default_level.load(std::memory_order_relaxed));
}

/**
 * @brief   获取指定模块的当前日志级别
 * @param   mod 模块名称
 * @return  该模块的日志级别
 */
LogLevel get_level(std::string_view mod)
{
    auto &reg = registry();
    std::lock_guard lock(reg.mutex);
    auto logger = ensure_module(reg, mod);
    return static_cast<LogLevel>(logger->level());
}

/**
 * @brief   检查是否应该输出指定级别的日志（无锁，热路径优化）
 * @param   level 要检查的日志级别
 * @return  true 应该输出；false 应该跳过
 */
bool should_log(LogLevel level) noexcept
{
    // LogLevel 枚举顺序即严重程度顺序，可与缓存阈值直接比较，避免加锁与格式化
    return static_cast<int>(level) >= registry().default_level.load(std::memory_order_relaxed);
}

/**
 * @brief   检查指定模块是否应该输出指定级别的日志
 * @param   mod 模块名称
 * @param   level 要检查的日志级别
 * @return  true 应该输出；false 应该跳过
 */
bool should_log(std::string_view mod, LogLevel level)
{
    auto &reg = registry();
    std::lock_guard lock(reg.mutex);
    return ensure_module(reg, mod)->should_log(to_spdlog_level(level));
}

/**
 * @brief   强制刷新所有已注册 logger 的缓冲区
 */
void flush()
{
    auto &reg = registry();
    std::lock_guard lock(reg.mutex);
    if (reg.default_logger)
        reg.default_logger->flush();
    for (auto &[name, logger] : reg.modules)
    {
        logger->flush();
    }
}

namespace detail
{

/**
 * @brief   输出日志到默认 logger（内部实现，带源码位置信息）
 * @param   level 日志级别
 * @param   msg 日志消息内容
 * @param   loc 源码位置（文件、行号、函数名）
 */
void log(LogLevel level, std::string_view msg, const SourceLoc &loc)
{
    auto &reg = registry();
    // 热路径：先尝试无锁读取已缓存的默认 logger
    auto logger = reg.default_cache.load(std::memory_order_acquire);
    if (!logger)
    {
        std::lock_guard lock(reg.mutex);
        logger = ensure_default(reg);
        reg.default_cache.store(logger, std::memory_order_release);
    }
    logger->log(spdlog::source_loc{loc.file, loc.line, loc.func}, to_spdlog_level(level), msg);
}

/**
 * @brief   输出日志到指定模块 logger（内部实现）
 * @param   mod 目标模块名称
 * @param   level 日志级别
 * @param   msg 日志消息内容
 * @param   loc 源码位置（文件、行号、函数名）
 */
void log(std::string_view mod, LogLevel level, std::string_view msg, const SourceLoc &loc)
{
    auto &reg = registry();
    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard lock(reg.mutex);
        logger = ensure_module(reg, mod);
    }
    logger->log(spdlog::source_loc{loc.file, loc.line, loc.func}, to_spdlog_level(level), msg);
}

} // namespace detail

} // namespace SZX::LOGGER
