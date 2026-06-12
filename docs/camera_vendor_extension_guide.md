# 新增厂家相机接入指南

这份文档说明如何在当前 `camera_core` 框架下接入新的相机厂家 SDK。  
整体思路是：把厂商 SDK 封装成 `ICameraSdkAdapter + ICameraSdkFactory`，其余业务层、`CameraService`、`VisionController` 都不需要知道底层厂家细节。

本文以 IKap 为参考实现。

## 1. 接入目标

新增一个厂家时，目标不是“把 SDK 调通”这么简单，而是把它纳入现有统一抽象：

- 上层仍只依赖 `CameraService`
- 单台设备仍表现为 `CameraDevice`
- 帧仍统一转换为 `FramePacket`
- 触发模式仍统一使用 `TriggerMode`
- 配置仍统一使用 `CameraConfig`

这样做的好处是：

- 后续业务层不需要为每个厂家写分支
- 不同厂家可以并行演进
- 测试和替换成本更低

## 2. 你需要实现的最小组件

一个新厂家通常至少需要这几块：

1. 一个 `ICameraSdkAdapter` 实现
2. 一个 `ICameraSdkFactory` 实现
3. 可选：一个“SDK 全局运行时管理器”
4. CMake 里的头文件/库依赖接入
5. 至少一个 discover/open/start 的基础测试

在 IKap 里，对应关系是：

- 适配器头文件：`include/camera/ikap/camera_ikapsdk_adapter.h`
- 适配器实现：`src/camera/ikap/camera_ikapsdk_adapter.cpp`
- SDK 运行时：`include/camera/ikap/ikap_runtime.h`
- SDK 运行时实现：`src/camera/ikap/ikap_runtime.cpp`

## 3. 先理解框架入口

新增厂商前，先明确上层到底依赖什么：

- `ICameraSdkAdapter`
  单台相机接口，负责 open / close / config / start / stop / trigger / frame callback
- `ICameraSdkFactory`
  全局工厂，负责 enumerateDevices 和 createAdapter
- `CameraManager`
  通过 factory 发现设备并创建 `CameraDevice`
- `CameraService`
  暴露给业务层的统一入口

所以你新增厂商时，核心不是去改 `CameraService`，而是实现好 adapter/factory。

## 4. 第一步：决定是否需要全局 SDK Runtime

很多厂家 SDK 都有“全局初始化 / 全局反初始化”概念，例如：

- `InitSdk()`
- `ShutdownSdk()`
- `CreateSystemHandle()`
- `ReleaseSystemHandle()`

如果你的 SDK 有这种约束，建议像 IKap 一样加一层运行时管理器。

IKap 的参考：

- `IkapSDKRuntime::acquire()`
- `IkapSDKRuntime::release()`

参考文件：

- `include/camera/ikap/ikap_runtime.h`
- `src/camera/ikap/ikap_runtime.cpp`

设计建议：

- 用单例
- 做引用计数
- 第一台设备进入时初始化 SDK
- 最后一台设备释放时终止 SDK

如果新厂家 SDK 没有全局初始化要求，这层可以省略。

## 5. 第二步：实现单台设备适配器

你需要继承 `ICameraSdkAdapter`。

接口定义见：

- `include/camera/core/camera_sdk_adapter.h`

必须实现的方法：

- `enumerateDevices()`
- `open()`
- `close()`
- `applyConfig()`
- `startGrabbing()`
- `stopGrabbing()`
- `executeSoftwareTrigger()`
- `setFrameCallback()`
- `info()`

### 推荐骨架

```cpp
class CameraFooSdkAdapter final : public SZXCamera::ICameraSdkAdapter
{
public:
    explicit CameraFooSdkAdapter(SZXCamera::CameraInfo info);
    ~CameraFooSdkAdapter() override;

    std::vector<SZXCamera::CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    bool open(std::string *errorMessage = nullptr) override;
    bool close(std::string *errorMessage = nullptr) override;
    bool applyConfig(const SZXCamera::CameraConfig &config, std::string *errorMessage = nullptr) override;
    bool startGrabbing(std::string *errorMessage = nullptr) override;
    bool stopGrabbing(std::string *errorMessage = nullptr) override;
    bool executeSoftwareTrigger(std::string *errorMessage = nullptr) override;
    void setFrameCallback(FrameCallback callback) override;
    SZXCamera::CameraInfo info() const override;

private:
    struct Impl;

private:
    SZXCamera::CameraInfo info_;
    std::unique_ptr<Impl> impl_;
};
```

### 推荐实现策略

- 对外只暴露统一接口
- 厂商句柄、stream、buffer、回调注册细节放进 `Impl`
- 把所有 SDK 错误转成字符串写入 `errorMessage`
- 适配器内部只关心“如何接入厂家”，不要混业务逻辑

IKap 参考点：

- `CameraIkapSdkAdapter::open()`
- `CameraIkapSdkAdapter::applyConfig()`
- `CameraIkapSdkAdapter::startGrabbing()`
- `CameraIkapSdkAdapter::executeSoftwareTrigger()`

## 6. 第三步：把厂家帧转换成统一 FramePacket

这是接入里最关键的一步。

你从厂商 SDK 拿到原始帧后，需要把它转换成：

- `FrameMeta`
- `width`
- `height`
- `channels`
- `pixelFormat`
- `data`

IKap 参考实现：

- `Impl::dispatch(...)`
- `Impl::make_frame(...)`

转换时建议保证：

- `meta.cameraName = logicalName`
- `meta.cameraSerialNumber = serialNumber`
- `meta.frameId` 自增
- `meta.timestamp = std::chrono::system_clock::now()`
- `data` 必须拷贝到统一缓冲区，避免底层 buffer 生命周期问题

### 示例

```cpp
bool CameraFooSdkAdapter::Impl::makeFrame(FooBuffer raw, SZXCamera::FramePacket &frame)
{
    frame = {};
    frame.meta.cameraName = boundInfo.logicalName;
    frame.meta.cameraSerialNumber = boundInfo.serialNumber;
    frame.meta.frameId = nextFrameId_++;
    frame.meta.timestamp = std::chrono::system_clock::now();

    frame.width = raw.width;
    frame.height = raw.height;
    frame.channels = 3;
    frame.pixelFormat = SZXCamera::PixelFormat::BGR8;

    frame.data.resize(raw.byteSize);
    std::memcpy(frame.data.data(), raw.data, raw.byteSize);
    return true;
}
```

## 7. 第四步：实现厂家工厂类

工厂负责两件事：

1. 枚举当前系统里有哪些设备
2. 为每个设备创建对应 adapter

接口定义仍在：

- `include/camera/core/camera_sdk_adapter.h`

### 推荐骨架

```cpp
class FooCameraSdkFactory final : public SZXCamera::ICameraSdkFactory
{
public:
    std::vector<SZXCamera::CameraInfo> enumerateDevices(std::string *errorMessage = nullptr) override;
    std::unique_ptr<SZXCamera::ICameraSdkAdapter> createAdapter(
        const SZXCamera::CameraInfo &info,
        std::string *errorMessage = nullptr) override;
};
```

### enumerateDevices 要做什么

- 调厂商 SDK 的全局枚举接口
- 为每台设备填充 `CameraInfo`
- 最少填这几个字段：
  - `deviceId`
  - `serialNumber`
  - `logicalName`
  - `vendor`
  - `model`

IKap 参考：

- `IkapCameraSdkFactory::enumerateDevices()`

它的做法是：

- 先 acquire runtime
- `ItkManGetDeviceCount()`
- `ItkManGetDeviceInfo()`
- 把结果映射成统一 `CameraInfo`

### createAdapter 要做什么

- 接收 `CameraInfo`
- 创建对应的 `CameraFooSdkAdapter`
- 如果失败，把异常或错误转成 `errorMessage`

IKap 参考：

- `IkapCameraSdkFactory::createAdapter()`

## 8. 第五步：实现参数映射

`CameraConfig` 是统一配置结构，但每家 SDK 的参数名和参数范围都不同，所以你要在 `applyConfig()` 里做翻译。

最常见的是：

- `exposureTime`
- `gain`
- `triggerMode`
- `bufferCount`

IKap 的参考在：

- `CameraIkapSdkAdapter::applyConfig()`

它当前做了这些事情：

- 根据 `bufferCount` 更新 stream buffer 数量
- 调整 trigger 模式
- 写曝光 `ExposureTime`
- 写增益 `DigitalGain`

### 建议做法

- 只映射你确认支持的字段
- 不支持的字段可以先忽略，或者写日志
- 失败时一定返回明确错误，不要静默吞掉

## 9. 第六步：处理触发模式

统一接口里的触发模式只有三种：

- `Continuous`
- `Software`
- `Hardware`

但厂商 SDK 对触发的特性名、枚举值、配置路径可能完全不同。

IKap 的处理值得参考，因为它不是简单写死参数，而是先探测相机支持哪套特性组合，再下发。

IKap 关键参考：

- `Impl::set_trigger_without_grabber(...)`

这个函数做了几件很实用的事：

- 先判断这台相机支持哪套触发配置路径
- 对 `TriggerSelector` 做候选值降级尝试
- 对外仍然只暴露统一 `TriggerMode`

这个思路很适合别的厂家：

- 不要把一个型号特性写死成全家通用
- 优先做“探测 + 回退”

## 10. 第七步：注册底层帧回调

SDK 一般会用两种方式给你图像：

- 回调模式
- 主动取流模式

无论哪种方式，最终目标都一样：

- 拿到原始帧
- 转成 `FramePacket`
- 调用 `frameCallback(sharedFrame)`

IKap 参考：

- `Impl::on_eof(...)`
- `Impl::dispatch(...)`

建议：

- 回调里只做轻量工作
- 真正复杂转换尽量控制在可预期范围
- 避免在回调线程里塞太多业务逻辑

## 11. 第八步：接入 CMake

一个新厂家接入通常要做两类事情：

### 头文件和库

在 CMake 里补厂家 SDK 的 include/lib/dll：

```cmake
set(FOO_ROOT "D:/third_party/foo_sdk")
set(FOO_INCLUDE_DIR "${FOO_ROOT}/include")
set(FOO_LIB_DIR "${FOO_ROOT}/lib/x64")

add_library(Foo::FooSdk SHARED IMPORTED GLOBAL)
set_target_properties(Foo::FooSdk PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FOO_INCLUDE_DIR}"
    IMPORTED_IMPLIB "${FOO_LIB_DIR}/FooSdk.lib"
)
```

### camera_core 链接

把新适配器源文件加入 `camera_core`，并链接厂家库：

```cmake
add_library(camera_core
    ...
    src/camera/foo/camera_foosdk_adapter.cpp
)

target_link_libraries(camera_core
    PUBLIC
        Foo::FooSdk
)
```

IKap 的参考是根目录 `CMakeLists.txt` 里：

- `IKAP_ROOT`
- `IKap::IKapC`
- `IKap::IKapBoard`
- `target_link_libraries(camera_core ... IKap::IKapC IKap::IKapBoard)`

## 12. 第九步：在应用入口选择工厂

接入完厂家后，上层只需要替换 factory。

例如当前 `apps/http_api_backend/main.cpp` 的做法是：

- 如果环境变量允许且 IKap 可用，就创建 `IkapCameraSdkFactory`
- 否则回退到 `FileImageCameraSdkFactory`

你新增厂家后，也可以这么做：

```cpp
std::shared_ptr<SZXCamera::ICameraSdkFactory> buildCameraFactory()
{
    const std::string mode = getenvOrDefault("CAMERA_FACTORY_MODE", "foo");

    if (mode == "foo") {
        return std::make_shared<SZXCamera::FooCameraSdkFactory>();
    }

    if (mode == "ikap") {
        return std::make_shared<SZXCamera::IkapCameraSdkFactory>();
    }

    return std::make_shared<SZXCamera::FileImageCameraSdkFactory>("camera_images", 5, true, 0);
}
```

## 13. 建议的目录结构

建议每个厂家保持独立子目录，避免混在一起。

示例：

```text
include/camera/foo/camera_foosdk_adapter.h
src/camera/foo/camera_foosdk_adapter.cpp

include/camera/ikap/camera_ikapsdk_adapter.h
src/camera/ikap/camera_ikapsdk_adapter.cpp
```

如果厂家 SDK 需要全局运行时，再加：

```text
include/camera/foo/foo_runtime.h
src/camera/foo/foo_runtime.cpp
```

## 14. 一个最小的新增厂家 Checklist

1. 先写 `FooCameraSdkFactory`
2. 再写 `CameraFooSdkAdapter`
3. 先让 `discover()` 可用
4. 再打通 `open()/close()`
5. 再打通 `applyConfig()`
6. 再打通 `startGrabbing()/stopGrabbing()`
7. 最后再补 `executeSoftwareTrigger()`
8. 补测试和日志

建议不要一开始就把所有功能一次性塞满，先把主链路跑通。

## 15. 常见坑

- 直接把厂商 SDK 类型暴露到业务层
- 回调里直接做重型业务逻辑
- 帧数据没拷贝，底层 buffer 释放后上层访问野指针
- 用 `logicalName` 当唯一设备 ID，导致设备重排后错绑
- 软件触发和连续模式切换时，没有先进入正确的采集状态
- CMake 里只加了 `.lib`，没把 include 目录和运行时 dll 处理好

## 16. 推荐先看的 IKap 参考点

如果你后面真要加新厂家，建议按下面顺序参考 IKap：

1. `include/camera/core/camera_sdk_adapter.h`
2. `include/camera/ikap/camera_ikapsdk_adapter.h`
3. `src/camera/ikap/ikap_runtime.cpp`
4. `src/camera/ikap/camera_ikapsdk_adapter.cpp`
5. `tests/test_camera_service_ikap.cpp`
6. `tests/test_camera_service_ikap_software.cpp`
