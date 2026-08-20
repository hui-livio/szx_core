/**
 * @file ikap_runtime.cc
 * @brief IKap SDK 运行时实现 — 负责 Windows 动态库加载和符号解析。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "ikap_runtime.h"

#ifdef _MSC_VER
    #pragma pack(push)
    #pragma warning(push)
    #pragma warning(disable : 4103)
#endif
#include "IKapC.h"
#ifdef _MSC_VER
    #pragma warning(pop)
    #pragma pack(pop)
#endif
#include "Logger.h"

#include <string>

using namespace SZX::LOGGER;

namespace
{
constexpr const char LOG_TAG[] = "Camera";
} // namespace

namespace SZXCamera
{

IkapSDKRuntime &IkapSDKRuntime::instance()
{
    static IkapSDKRuntime runtime;
    return runtime;
}

bool IkapSDKRuntime::acquire(std::string *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_)
    {
        ++refCount_;
        LOGM_DEBUG(LOG_TAG, "IKap runtime acquire success, already initialized, ref_count={}", refCount_);
        return true;
    }

    LOGM_INFO(LOG_TAG, "IKap runtime first acquire, initializing SDK");
    const int status = ItkManInitialize();
    if (status != ITKSTATUS_OK)
    {
        if (errorMessage)
            *errorMessage = "ItkManInitialize failed, status=" + std::to_string(status);
        LOGM_ERROR(LOG_TAG, "IKap runtime initialize failed, status={}", status);
        return false;
    }

    initialized_ = true;
    refCount_    = 1;

    LOGM_INFO(LOG_TAG, "IKap runtime initialized successfully, ref_count={}", refCount_);
    return true;
}

void IkapSDKRuntime::release()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (refCount_ <= 0)
    {
        LOGM_WARN(LOG_TAG, "IKap runtime release called with ref_count <= 0");
        return;
    }

    --refCount_;
    LOGM_DEBUG(LOG_TAG, "IKap runtime release called, ref_count={}", refCount_);

    if (refCount_ > 0)
        return;

    if (initialized_)
    {
        LOGM_INFO(LOG_TAG, "IKap runtime last release, terminating SDK");
        const int status = ItkManTerminate();
        if (status != ITKSTATUS_OK)
        {
            LOGM_ERROR(LOG_TAG, "IKap runtime terminate failed, status={}", status);
            return;
        }
        initialized_ = false;
        LOGM_INFO(LOG_TAG, "IKap runtime terminated successfully");
    }
}

bool IkapSDKRuntime::isInitialized() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

int IkapSDKRuntime::refCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return refCount_;
}

} // namespace SZXCamera
