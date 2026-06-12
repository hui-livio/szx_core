#pragma once

#include "camera/camera_core_api.h"

#include <mutex>
#include <string>

namespace SZXCamera{
    /**
     * IKap SDK 全局运行时管理器。
     *
     * 职责：
     * 1. 保证 IKap SDK 在整个进程内只初始化一次。
     * 2. 保证 IKap SDK 在最后一个使用者释放后只终止一次。
     * 3. 对上层屏蔽 ItkManInitialize / ItkManTerminate 的全局生命周期细节。
     *
     * 设计说明：
     * - 这是一个进程级单例。
     * - 通过 acquire/release 做引用计数管理。
     * - 适合多个相机适配器共享同一个 SDK 运行时。
     */
    class CAMERA_CORE_API IkapSDKRuntime
    {
    public:
        /**
         * 获取全局单例。
         */
        static IkapSDKRuntime& instance();

        /**
         * 获取一次 IKap SDK 使用权。
         *
         * 行为：
         * - 当第一个调用者进入时，会触发 ItkManInitialize()。
         * - 后续调用者只增加引用计数，不重复初始化。
         *
         * @param errorMessage 输出错误信息。
         * @return true 成功获取 SDK 使用权。
         * @return false 获取失败。
         */
        bool acquire(std::string* errorMessage = nullptr);

        /**
         * 释放一次 IKap SDK 使用权。
         *
         * 行为：
         * - 引用计数减 1。
         * - 当引用计数降为 0 时，触发 ItkManTerminate()。
         */
        void release();

        /**
         * 查询当前 SDK 是否已初始化成功。
         */
        bool isInitialized() const;

        /**
         * 当前引用计数，仅用于调试或日志观察。
         */
        int refCount() const;

    private:
        IkapSDKRuntime() = default;
        ~IkapSDKRuntime() = default;

        IkapSDKRuntime(const IkapSDKRuntime&) = delete;
        IkapSDKRuntime& operator=(const IkapSDKRuntime&) = delete;

    private:
        mutable std::mutex mutex_;
        int refCount_{0};
        bool initialized_{false};
    };

}
