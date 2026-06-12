# camera_core 使用说明

`camera_core` 是相机子系统对外的核心库，目标是把“设备接入、相机生命周期、帧分发”统一收口到一套稳定接口里，让上层业务只依赖通用类型和服务入口，而不是直接依赖厂商 SDK。

## 1. 模块分层

- `ICameraSdkFactory`
  负责枚举设备、为每台设备创建具体适配器。
- `ICameraSdkAdapter`
  单台相机和具体 SDK 的适配层。
- `CameraDevice`
  单台相机的统一封装，负责 open / close / applyConfig / start / stop。
- `CameraManager`
  多相机管理器，负责 discover 和按序列号查询设备。
- `CapturePipeline`
  帧发布订阅管道，负责把相机输出帧分发给 sink。
- `CameraService`
  相机子系统的对外统一入口，业务层建议优先依赖它。

## 2. 关键数据类型

主要定义在 `include/camera/core/camera_types.h`。

- `CameraInfo`
  描述一台相机是谁，包含 `deviceId`、`serialNumber`、`logicalName`、`vendor`、`model`、`role`。
- `CameraConfig`
  描述一台相机如何工作，包含 `exposureTime`、`gain`、`triggerMode`、`pixelFormat`、`bufferCount`、`reverseX`、`reverseY`。
- `CameraRuntimeState`
  描述当前运行状态，包含 `state`、`lastError`、`totalFrames`、`lastFrameTime`、延迟统计。
- `FrameMeta`
  一帧的业务元数据，包含 `cameraName`、`cameraSerialNumber`、`frameId`、`timestamp`、`height`、`taskId`。
- `FramePacket`
  统一帧结构，包含宽高、通道数、像素格式和原始数据。

### 相机状态

- `Created`
- `Discovered`
- `Opened`
- `Configured`
- `Streaming`
- `Stopped`
- `Error`

### 触发模式

- `TriggerMode::Continuous`
  连续采集。
- `TriggerMode::Software`
  软件触发。
- `TriggerMode::Hardware`
  硬件触发。

## 3. 标准生命周期

推荐调用顺序：

1. 创建 `ICameraSdkFactory`
2. 用 factory 创建 `CameraService`
3. `discover()`
4. `initialize()`
5. `applyCameraConfigs()`
6. 可选：注册 sink
7. `startCamerasBySerialNumbers()`
8. 可选：`executeSoftwareTriggerBySerialNumbers()`
9. `stopCamerasBySerialNumbers()`
10. `shutdown()`

说明：

- `discover()` 会重新扫描并重建内部设备列表。
- `initialize()` 当前语义主要是“打开全部相机”。
- `applyCameraConfigs()` 以序列号为 key 批量下发配置。
- `start/stop` 是按序列号选择性启动和停止。

## 4. 最小示例：文件图片模式

当机器上没有真实相机时，可以先用 `FileImageCameraSdkFactory` 跑通流程。

```cpp
#include "camera/core/camera_service.h"
#include "camera/filecamera/camera_fileimage_sdk_adapter.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

int main()
{
    auto factory = std::make_shared<SZXCamera::FileImageCameraSdkFactory>(
        "camera_images",  // 根目录，里面可以是 camera_0/camera_1 子目录
        5,                // 播放帧率
        true,             // 是否循环
        0                 // 0 表示不限帧数
    );

    auto service = std::make_shared<SZXCamera::CameraService>(factory);

    std::string error;
    if (!service->discover(&error)) {
        std::cerr << "discover failed: " << error << std::endl;
        return 1;
    }

    const auto cameras = service->cameras();
    for (const auto &camera : cameras) {
        std::cout << "serial=" << camera.serialNumber
                  << ", logical=" << camera.logicalName << std::endl;
    }

    if (!service->initialize(&error)) {
        std::cerr << "initialize failed: " << error << std::endl;
        return 1;
    }

    std::unordered_map<std::string, SZXCamera::CameraConfig> configs;
    for (const auto &camera : cameras) {
        SZXCamera::CameraConfig config;
        config.exposureTime = 1000.0;
        config.gain = 1.0;
        config.triggerMode = SZXCamera::TriggerMode::Continuous;
        config.bufferCount = 8;
        configs.emplace(camera.serialNumber, config);
    }

    if (!service->applyCameraConfigs(configs, &error)) {
        std::cerr << "apply config failed: " << error << std::endl;
        return 1;
    }

    std::vector<std::string> serials;
    for (const auto &camera : cameras) {
        serials.push_back(camera.serialNumber);
    }

    if (!service->startCamerasBySerialNumbers(serials, &error)) {
        std::cerr << "start failed: " << error << std::endl;
        return 1;
    }

    // 这里可以继续等待帧、做算法、写盘等

    service->stopCamerasBySerialNumbers(serials, nullptr);
    service->shutdown(nullptr);
    return 0;
}
```

## 5. 存图示例：SaveImageConsumer

`SaveImageConsumer` 是一个异步 sink，适合直接把采集帧落盘。

```cpp
#include "camera/core/camera_service.h"
#include "camera/filecamera/camera_fileimage_sdk_adapter.h"
#include "camera/sinks/save_image_consumer.h"

auto factory = std::make_shared<SZXCamera::FileImageCameraSdkFactory>("camera_images", 5, true, 0);
auto service = std::make_shared<SZXCamera::CameraService>(factory);

auto saveSink = std::make_shared<SZXCamera::SaveImageConsumer>("save_images", 100);
saveSink->start();
service->addFrameSink(saveSink);

std::string error;
service->discover(&error);
service->initialize(&error);

const auto cameras = service->cameras();
std::vector<std::string> serials;
for (const auto &camera : cameras) {
    serials.push_back(camera.serialNumber);
}

service->startCamerasBySerialNumbers(serials, &error);

// 业务运行...

service->stopCamerasBySerialNumbers(serials, nullptr);
service->shutdown(nullptr);
saveSink->stop();
```

说明：

- `SaveImageConsumer::consume()` 只做快速入队，不直接写盘。
- 队列满时会丢帧，不会反向阻塞相机采集线程。
- 建议先 `start()`，结束时记得 `stop()`。

## 6. 订阅式 sink 示例

`CapturePipeline` 支持按相机、任务 ID、高曝光等条件分发。

```cpp
auto sink = std::make_shared<MyFrameSink>();

SZXCamera::FrameSubscription subscription;
subscription.cameraNames = {"camera_0"};
subscription.onlyHighExposure = true;

service->addFrameSink(sink, subscription);
```

匹配规则：

- 所有已填写条件都按 AND 逻辑匹配。
- 不填条件则表示接收全部帧。

## 7. IKap 模式示例

如果已安装 IKap SDK，可以使用 `IkapCameraSdkFactory`。

```cpp
#include "camera/core/camera_service.h"
#include "camera/ikap/camera_ikapsdk_adapter.h"

auto factory = std::make_shared<SZXCamera::IkapCameraSdkFactory>();
auto service = std::make_shared<SZXCamera::CameraService>(factory);

std::string error;
service->discover(&error);
service->initialize(&error);
```

软件触发模式示例：

```cpp
SZXCamera::CameraConfig config;
config.triggerMode = SZXCamera::TriggerMode::Software;
config.exposureTime = 1000.0;
config.gain = 1.0;
config.bufferCount = 8;

std::unordered_map<std::string, SZXCamera::CameraConfig> configs;
configs.emplace(targetSerial, config);
service->applyCameraConfigs(configs, &error);

service->startCamerasBySerialNumbers({targetSerial}, &error);
service->executeSoftwareTriggerBySerialNumbers({targetSerial}, &error);
```

注意：

- 软件触发前，相机通常需要先 `start()` 进入等待触发状态。
- 真实行为受厂商 SDK、设备参数和接线方式影响。

## 8. 状态和统计信息

`CameraService` 提供两类常用观测接口：

- `cameraRuntimeStates()`
  返回每台相机当前运行态。
- `pipelineStats()`
  返回 pipeline 的累计统计信息。

示例：

```cpp
const auto runtimeStates = service->cameraRuntimeStates();
const auto stats = service->pipelineStats();
```

常见关注项：

- 单台相机是否进入 `Streaming`
- `lastError` 是否为空
- `totalFrames` 是否持续增长
- `publishedFrames` / `deliveredFrames` 是否增长

## 9. 常见注意事项

- 不要在相机正在 `Opened / Configured / Streaming` 时重复 `discover()`，当前实现会拒绝。
- `FileImageCameraSdkAdapter` 不支持软件触发。
- 如果 pipeline 中的 sink 很耗时，应该在 sink 内部自行异步化。
- 如果业务层需要“部分配置更新”，建议在上层先做配置 merge，再下发完整 `CameraConfig`。
- 相机配置的单位和有效范围最终以具体 SDK / 设备为准。

## 10. 相关文件

- `include/camera/core/camera_service.h`
- `include/camera/core/camera_types.h`
- `include/camera/core/camera_pipeline.h`
- `include/camera/filecamera/camera_fileimage_sdk_adapter.h`
- `include/camera/sinks/save_image_consumer.h`
- `tests/test_camera_fileimage_sdk_adapter.cpp`
- `tests/test_camera_service_ikap.cpp`
- `tests/test_camera_service_ikap_software.cpp`

## 11. 扩展阅读

如果你需要新增新的厂家 SDK 接入方式，可以继续看：

- `docs/camera_vendor_extension_guide.md`
