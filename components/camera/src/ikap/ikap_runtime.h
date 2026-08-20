/**
 * @file ikap_runtime.h
 * @brief IKap SDK 运行时封装 — 管理动态库加载和函数入口。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once


#include <mutex>
#include <string>


namespace SZXCamera
{

// ========================================================================
// IkapSDKRuntime — IKap SDK 全局运行时单例（内部类，不导出）
// ========================================================================

class IkapSDKRuntime
{
public:
    static IkapSDKRuntime &instance();

    bool acquire(std::string *errorMessage = nullptr);
    void release();
    bool isInitialized() const;
    int refCount() const;

private:
    IkapSDKRuntime()  = default;
    ~IkapSDKRuntime() = default;

    IkapSDKRuntime(const IkapSDKRuntime &)            = delete;
    IkapSDKRuntime &operator=(const IkapSDKRuntime &) = delete;

    mutable std::mutex mutex_;
    int refCount_{0};
    bool initialized_{false};
};

} // namespace SZXCamera
