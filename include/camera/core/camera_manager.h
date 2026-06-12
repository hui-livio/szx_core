#pragma once

#include "camera_device.h"
#include "camera_sdk_adapter.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SZXCamera {
    /**
     * CameraManager 负责多台相机的统一管理。
     *
     * 它的职责包括：
     * - 调用工厂枚举设备
     * - 为每台设备创建 CameraDevice
     * - 按 serialNumber 查询设备
     * - 批量开关和批量配置
     *
     * 不承担具体业务流程编排。
     */
    
     class CameraManager
    {
        public:
            /**
             * 构造多相机管理器。
             *
             * @param factory 相机 SDK 工厂对象。
             *        Manager 通过它完成设备枚举和适配器创建。
             */
            explicit CameraManager(std::shared_ptr<ICameraSdkFactory> factory);

            /**
             * 扫描并重建当前相机列表。
             *
             * 功能：
             * - 调用工厂枚举所有可见设备。
             * - 为每台设备创建一个 CameraDevice。
             * - 用新的相机列表替换当前缓存。
             *
             * @param errorMessage 输出参数。
             *        当扫描失败或建设备失败时，写入可读错误信息。
             *
             * @return true 表示扫描成功。
             * @return false 表示扫描失败。
             */
            bool discover(std::string *errorMessage = nullptr);

            /**
             * 获取最近一次扫描到的设备信息列表。
             *
             * @return std::vector<CameraInfo> 相机基础信息列表副本。
             */
            std::vector<CameraInfo> listDiscovered() const;

            /**
             * 按序列号获取一台相机设备对象。
             *
             * @param serialNumber 相机序列号。
             *
             * @return std::shared_ptr<CameraDevice> 找到则返回设备对象，否则返回 nullptr。
             */
            std::shared_ptr<CameraDevice> getCameraBySerialNumber(const std::string &serialNumber) const;

            /**
             * 获取当前管理的所有设备对象。
             *
             * @return std::vector<std::shared_ptr<CameraDevice>> 所有设备对象的列表副本。
             */
            std::vector<std::shared_ptr<CameraDevice>> listCameras() const;

            /**
             * 批量打开所有相机。
             *
             * 功能：
             * - 依次调用每台相机的 open。
             * - 一旦某台失败，立即返回失败。
             *
             * @param errorMessage 输出参数。
             *        某台相机打开失败时写入对应错误信息。
             *
             * @return true 表示全部打开成功。
             * @return false 表示存在至少一台相机打开失败。
             */
            bool openAll(std::string *errorMessage = nullptr);

            /**
             * 批量关闭所有相机。
             *
             * 功能：
             * - 依次调用每台相机的 close。
             * - 一旦某台失败，立即返回失败。
             *
             * @param errorMessage 输出参数。
             *        某台相机关闭失败时写入对应错误信息。
             *
             * @return true 表示全部关闭成功。
             * @return false 表示存在至少一台相机关闭失败。
             */
            bool closeAll(std::string *errorMessage = nullptr);

        private:
            mutable std::mutex mutex_;   ///< 保护设备信息列表和序列号映射表的并发访问。
            std::shared_ptr<ICameraSdkFactory> factory_;  ///< 用于枚举设备和创建设备适配器的工厂对象。
            std::vector<CameraInfo> discovered_;          ///< 最近一次 discover 得到的设备基础信息列表。
            std::unordered_map<std::string, std::shared_ptr<CameraDevice>> cameras_;  ///< 序列号到设备对象的映射表。
    };

}
