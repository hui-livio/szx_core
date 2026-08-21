# szx_core 设计文档

本文档说明 szx_core 这个库整体如何分层、模块之间的依赖方向、关键的抽象边界，以及作为通用库的扩展点。看完这份就理解"为什么我能用它，应该在哪里 hook 自己的代码"。

模块级设计：

- camera_core 的类设计：[camera_core_class_design.md](camera_core_class_design.md)
- camera_core 的扩展方法：[camera_vendor_extension_guide.md](../使用/camera_vendor_extension_guide.md)

## 1. 设计目标

1. **业务层不感知硬件**：上层只依赖中立数据结构和服务入口，不接触厂商 SDK 的句柄、回调签名、错误码。
2. **新增厂商成本可控**：接入一家新相机 SDK，只需要实现两个接口（适配器 + 工厂），其它层不动。
3. **三大模块可独立使用**：相机、PLC、日志互相独立，业务可以只用其中一个；它们共享日志，但不互相依赖。
4. **构建可降级**：缺少厂商 SDK / libmodbus 时，对应模块/适配器自动跳过编译，不阻断核心功能。
5. **作为静态/动态库都能用**：组件按各自 CMake target 定义库类型，统一通过 `SZX_COMPONENTS_INSTALL` 控制安装；不再使用 `SZX_CORE_*` 旧选项。

## 2. 三大模块及其依赖

```
┌────────────────────────────────────────────────────────────┐
│                         你的应用                            │
└────────────┬─────────────────┬──────────────────┬──────────┘
             │                 │                  │
             ▼                 ▼                  ▼
    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
    │ camera_core │    │  plc_core   │    │common_supp. │
    └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
           │                  │                  │
           │                  │                  ▼
           │                  │           ┌─────────────┐
           │                  │           │   spdlog    │
           │                  │           └─────────────┘
           ▼                  ▼
   ┌───────────────┐  ┌───────────────┐
   │ 厂商 SDK 们   │  │   libmodbus   │
   │ (IKap/Huaray) │  │               │
   └───────────────┘  └───────────────┘
```

依赖方向严格自上而下，禁止反向依赖。

- **common_support** 是地基，依赖 spdlog（PUBLIC，向上透出 logger API）。
- **camera_core** 依赖 common_support；可选依赖一个或多个厂商 SDK，取决于 CMake 探测结果。
- **plc_core** 依赖 common_support 和 libmodbus。
- camera_core 和 plc_core **互不依赖** —— 你可以只用其中一个。

## 3. common_support 设计

只有两块内容：日志封装、阻塞队列工具。

### 3.1 日志（`common/log/log_manager.h` 和 `log_macros.h`）

- `LogManager` 是单例，负责初始化 spdlog logger 和 sink（文件 + console）。
- `LogOptions` 配置 logger 名、文件路径、是否启用 console、日志级别。
- `LOG_INFO / LOG_DEBUG / LOG_WARN / LOG_ERROR` 宏使用项目默认 logger。

设计要点：

- 业务层只引 `log_macros.h`，不直接 include `spdlog/spdlog.h` —— 这样将来换日志库只改一处。
- 但 `LogManager::init()` 必须显式调一次，否则宏会写到默认 console。

### 3.2 阻塞队列（`common/queue/blocking_queue.h`、`bounded_blocking_queue.h`）

- 模板类，线程安全 producer-consumer。
- `BoundedBlockingQueue<T>` 带容量上限，满了可以阻塞 / 丢弃 / 抛异常（构造时选策略）。

设计目的：让相机回调线程把帧丢进队列后立即返回，重活由 worker 线程处理。任何业务里的 sink 都建议套这个模式。

## 4. camera_core 设计

详细见 [camera_core_class_design.md](camera_core_class_design.md)。这里给一个 30 秒版本：

```
┌──────────────────────────────────────────────┐
│            CameraService（业务入口）          │
├──────────────────────────────────────────────┤
│ CameraManager(多机)  │ CapturePipeline(分发) │
├──────────────────────────────────────────────┤
│            CameraDevice（单机, 状态机）       │
├──────────────────────────────────────────────┤
│ ICameraSdkAdapter ← ICameraSdkFactory       │ ← 厂商防火墙
├──────────────────────────────────────────────┤
│  FileImage / IKap / Huaray-FG / Huaray-USB  │
└──────────────────────────────────────────────┘
```

### 4.1 关键的中立数据类型（`camera/core/camera_types.h`）

`CameraInfo / CameraConfig / CameraRuntimeState / FrameMeta / FramePacket` —— 业务层只用这些，不碰厂商类型。enum 用 `CameraRole / CameraState / TriggerMode / PixelFormat`。

### 4.2 厂商防火墙（`camera/core/camera_sdk_adapter.h`）

两个纯虚接口：

- `ICameraSdkAdapter`：一台相机和一个具体 SDK 的适配器。9 个方法：`enumerateDevices / open / close / applyConfig / startGrabbing / stopGrabbing / executeSoftwareTrigger / setFrameCallback / info`。
- `ICameraSdkFactory`：全局枚举 + 适配器工厂。2 个方法：`enumerateDevices / createAdapter`。

新增厂商只需要实现这两个，参考 [camera_vendor_extension_guide.md](../使用/camera_vendor_extension_guide.md)。

### 4.3 帧分发（`camera/core/camera_pipeline.h`）

- `CapturePipeline` 是简单的同步发布-订阅：`publish(frame)` 同步遍历所有 sink，匹配订阅条件就调 `sink->consume(frame)`。
- `FrameSubscription` 描述 sink 关注哪些帧：按相机序列号 / 逻辑名 / 任务 ID / 高曝光过滤。空条件 = 收所有帧。
- **关键约束**：`consume()` 跑在 SDK 回调线程，sink 必须快速返回。重活由 sink 自己异步化。

### 4.4 内置适配器

| 适配器 | 类 | 适用 | 编译条件 |
|---|---|---|---|
| FileImage | `FileImageCameraSdkFactory` | 假相机，回放图片目录 | 始终编译 |
| IKap | `IkapCameraSdkFactory` | I-TEK 帧采集卡 | `SZX_CORE_HAS_IKAP` |
| Huaray-FG | `HuarayCameraSdkFactory` | 华睿帧采集卡（IMVFG） | `SZX_CORE_HAS_HUARAY` |
| Huaray-USB | `HuarayUsbCameraSdkFactory` | 华睿 USB/GigE 直连（IMV） | `SZX_CORE_HAS_HUARAY` |

CMake 探测到对应 SDK 时自动启用，无需手工开关。

## 5. plc_core 设计

`PlcController` 封装 libmodbus，提供：

- 连接管理（TCP / RTU）
- 寄存器读写（线圈、保持寄存器、输入寄存器）
- 异常时统一的错误返回

接口设计参考 `include/plc/plccontrol.h` 和 [plc_core_usage.md](../使用/plc_core_usage.md)。

设计目的：业务层不直接调 libmodbus，而是面向 `PlcController`，这样将来换底层 PLC 通信库（比如换成 OPC UA）只改一处。

## 6. 扩展点

| 想做什么 | 在哪里改 | 看哪份文档 |
|---|---|---|
| 接入新厂商相机 | 实现 `ICameraSdkAdapter` + `ICameraSdkFactory` | [camera_vendor_extension_guide.md](../使用/camera_vendor_extension_guide.md) |
| 自定义帧 sink（存盘、推流、检测） | 实现 `IFrameSink` 注册到 service | [camera_core_usage.md](../使用/camera_core_usage.md) §6 订阅式 sink |
| 自定义订阅过滤 | 用 `FrameSubscription` 的字段组合 AND 匹配；不够用就在 sink 内部自己再过滤 | [camera_core_usage.md](../使用/camera_core_usage.md) §6 |
| 换日志后端 | 改 common_support/log_manager 的 sink 创建逻辑，宏不动 | `common/log/log_macros.h` |
| 加新的 PLC 寄存器类型 | 在 `PlcController` 加方法，封装 libmodbus 对应调用 | `include/plc/plccontrol.h` |

## 7. 关键设计决策（FAQ）

| 决策 | 原因 |
|---|---|
| 用序列号而不是 logicalName 作为相机唯一 key | logicalName 可能因设备重排错绑；序列号是设备稳定标识 |
| 帧用 `shared_ptr<const FramePacket>` 传递 | 多 sink 共享同一帧；只读保证语义安全 |
| `publish()` 同步分发 | 简单可控；耗时逻辑下放给 sink 自己异步 |
| 状态机集中在 `CameraDevice.updateState` | 单点更新，便于加锁和排障 |
| `CameraManager.discover()` 重建而非增量 | 避免"老对象残留 + 新对象重复"的难调试问题 |
| 单独抽 `ICameraSdkFactory` 不塞到 adapter | 枚举是 SDK 的全局行为，不是单机行为 |
| 错误用 `bool + string* errorMessage` 双返回 | C++ 端避免异常跨 SDK 边界；上层若想用异常可在边界封装 |
| 缺 SDK 时 adapter 不编译而不是 stub | 上层代码引用了 stub 时不会"在运行时才发现没编译"，编译期就报错；也避免无意义的链接膨胀 |
| 不内置 OpenCV 依赖 | 让纯相机/PLC 用户不背 OpenCV 包袱；存图等需求由用户在自己工程加 sink 实现 |

## 8. 不在库范围内的东西

- **HTTP / API 服务**：属于应用层，每个项目协议不同，不放进通用库。
- **业务编排（VisionController 之类）**：业务流程因项目而异。
- **OpenCV 相关 sink（存盘、编码、显示）**：由消费方实现，避免强依赖。
- **算法**：检测、拼图、标定都属于业务。

如果你需要这些，参考 `nanrui/camera_control_http` 工程的 `vision_app_core` 模块作为示例。

## 9. 版本约定

`MAJOR.MINOR.PATCH`（SemVer）。当前 0.x 阶段，API 可能有破坏性变更，发布到 1.0 后保证 MAJOR 内 ABI 稳定。
