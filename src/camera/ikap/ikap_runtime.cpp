#include "camera/ikap/ikap_runtime.h"
#include "IKapC.h"
#include "common/log/log_macros.h"

#include <string>


namespace SZXCamera{
    namespace
    {
        bool isIkapSuccess(int status){
            return status == ITKSTATUS_OK;
        }
    }

    IkapSDKRuntime& IkapSDKRuntime::instance()
    {
        static IkapSDKRuntime runtime;
        return runtime;
    }

    bool IkapSDKRuntime::acquire(std::string* errorMessage)
    {
        if (errorMessage) {
            errorMessage->clear();
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 已经初始化过，直接增加引用计数
        if (initialized_) {
            ++refCount_;
            LOG_DEBUG("IKap runtime acquire success, already initialized, ref_count={}", refCount_);
            return true;
        }

        // 第一次使用，执行全局初始化
        LOG_INFO("IKap runtime first acquire, initializing SDK");
        const int status = ItkManInitialize();
        if (!isIkapSuccess(status)) {
            if (errorMessage) {
                *errorMessage = "ItkManInitialize failed, status=" + std::to_string(status);
            }
            LOG_ERROR("IKap runtime initialize failed, status={}", status);
            return false;
        }

        initialized_ = true;
        refCount_ = 1;

        LOG_INFO("IKap runtime initialized successfully, ref_count={}", refCount_);
        return true;
    }

    void IkapSDKRuntime::release()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (refCount_ <= 0) {
            LOG_WARN("IKap runtime release called with ref_count <= 0");
            return;
        }

        --refCount_;
        LOG_DEBUG("IKap runtime release called, ref_count={}", refCount_);

        // 还有其他使用者，不做 terminate
        if (refCount_ > 0) {
            return;
        }

        // 最后一个使用者释放，执行全局终止
        if (initialized_) {
            LOG_INFO("IKap runtime last release, terminating SDK");
            const int status = ItkManTerminate();
            if (!isIkapSuccess(status)) {
                LOG_ERROR("IKap runtime terminate failed, status={}", status);
                return;
            }
            initialized_ = false;
            LOG_INFO("IKap runtime terminated successfully");
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

}