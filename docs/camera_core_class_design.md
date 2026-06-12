# camera_core 类设计

本文用与语言无关的方式描述 `camera_core` 的类设计，目标是：

- 让任何新成员一眼看清楚“谁依赖谁、各层职责到哪里为止”。
- 作为 Python 端复刻同一套硬件封装的参考蓝图，确保 C++ 与 Python 两端在数据模型、生命周期、错误语义上保持一致。

如果要看怎么用，请看 [camera_core_usage.md](camera_core_usage.md)；如果要新增厂商接入，请看 [camera_vendor_extension_guide.md](camera_vendor_extension_guide.md)。

## 1. 设计目标

1. 上层业务（HTTP / VisionController / 算法模块）只依赖中立数据结构和 `CameraService`，不接触任何厂商 SDK 的句柄、回调签名、错误码。
2. 新增厂家只需要写两个类（`ICameraSdkAdapter` + `ICameraSdkFactory`），其它层不动。
3. 单台相机表现为一个有明确状态机的对象，便于错误恢复和并发控制。
4. 帧数据用“发布-订阅”管道流转，设备层只负责拿帧，处理层只负责消费帧，互不耦合。
5. 同一套类设计在 C++ 和 Python 两端都能落地，差异收敛在“线程模型、错误返回风格”这种语言层面。

## 2. 整体分层

```
┌──────────────────────────────────────────────────────┐
│  CameraService              ← 业务层唯一入口          │
├──────────────────────────────────────────────────────┤
│  CameraManager      │     CapturePipeline             │
│  (多机管理)          │     (发布-订阅, 帧分发)         │
├──────────────────────────────────────────────────────┤
│                CameraDevice                            │ ← 单机统一封装(状态机)
├──────────────────────────────────────────────────────┤
│  ICameraSdkAdapter      ←      ICameraSdkFactory       │ ← 与厂商 SDK 的隔离边界
├──────────────────────────────────────────────────────┤
│   IKap / FileImage / 海康 / ...                        │ ← 厂商实现
└──────────────────────────────────────────────────────┘
```

依赖方向自上而下，禁止反向依赖。`CameraService` 是业务唯一入口，下面所有层都不应被业务直接 import。

## 3. 数据类型层

定义在 [camera_types.h](../include/camera/core/camera_types.h)。这是整套设计里最“稳定”的部分，**所有跨层数据都用这些结构传递**。

Python 侧建议直接用 `enum.Enum` + `@dataclass`。

### 3.1 枚举

| 枚举 | 取值 | 含义 |
|---|---|---|
| `CameraRole` | `Unknown / Top / Side / External / Other` | 业务角色，跟厂商无关 |
| `CameraState` | `Created → Discovered → Opened → Configured → Streaming → Stopped`，加 `Error` | 设备生命周期 |
| `TriggerMode` | `Continuous / Software / Hardware` | 触发模式 |
| `PixelFormat` | `Unknown / Mono8 / RGB8 / BGR8` | 像素格式统一抽象 |

### 3.2 值对象

| 结构 | 角色 | 关键字段 |
|---|---|---|
| `CameraInfo` | 这台相机“是谁” | `deviceId, serialNumber, logicalName, vendor, model, role` |
| `CameraConfig` | 这台相机“怎么工作” | `exposureTime, gain, triggerMode, pixelFormat, bufferCount, reverseX, reverseY` |
| `CameraRuntimeState` | 这台相机“现在跑成啥样” | `state, lastError, totalFrames, lastFrameTime, lastFrameLatencyUs, maxFrameLatencyUs` |
| `FrameMeta` | 帧的业务上下文 | `cameraName, cameraSerialNumber, frameId, timestamp, height, highExposure, taskId` |
| `FramePacket` | 帧的统一承载 | `meta, width, height, channels, pixelFormat, data(bytes)` |
| `FramePtr` | 帧的共享只读引用 | C++ 是 `shared_ptr<const FramePacket>`；Python 用别名 + 不修改约定 |

### 3.3 设计约束

- 业务层 **只能** 读 `FramePacket`，不应修改。任何对帧的派生处理都应产生新对象。
- `CameraInfo.serialNumber` 是设备唯一稳定标识，所有按设备查询/启停接口都以序列号为 key，**不要用 logicalName 做绑定**（会因为设备重排而错绑）。
- `CameraConfig` 的字段值最终含义由具体厂商 SDK 决定（例如曝光单位通常是 us，但请以 SDK 文档为准）。

## 4. SDK 隔离层

这一层是“厂商防火墙”。新增厂家时，唯一需要写的就是这两个接口。

### 4.1 `ICameraSdkAdapter`：一台相机 ↔ 一个厂商 SDK

定义在 [camera_sdk_adapter.h](../include/camera/core/camera_sdk_adapter.h)。每个实例只负责一台相机。

| 方法 | 职责 |
|---|---|
| `enumerateDevices()` | 调厂商全局枚举接口，输出统一 `CameraInfo` 列表 |
| `open()` / `close()` | 连接 / 断开设备 |
| `applyConfig(CameraConfig)` | 把统一配置翻译成厂商参数 |
| `startGrabbing()` / `stopGrabbing()` | 开始 / 停止出图（连续模式打开数据流；触发模式进入等待） |
| `executeSoftwareTrigger()` | 软件触发一次；不支持时返回明确错误 |
| `setFrameCallback(cb)` | 注册帧回调，将厂商原始帧转成 `FramePacket` 后推给上层 |
| `info()` | 返回该相机的 `CameraInfo` |

**实现约束**：

- 帧数据**必须拷贝**到统一缓冲区，避免厂商 buffer 释放后野指针。
- 回调里只做轻量转换，重业务一律推给上层。
- 所有厂商错误码都翻译成可读的 `errorMessage`，不要静默吞掉。

### 4.2 `ICameraSdkFactory`：全局枚举 + 适配器工厂

| 方法 | 职责 |
|---|---|
| `enumerateDevices()` | 调厂商全局枚举（通常对应 SDK 的“列出所有设备”接口） |
| `createAdapter(CameraInfo)` | 为指定设备创建一个 `ICameraSdkAdapter` 实例 |

### 4.3 可选：SDK Runtime 单例

如果厂商 SDK 有“全局 Init / Shutdown”约束（例如 IKap 的 [ikap_runtime.h](../include/camera/ikap/ikap_runtime.h)），加一层引用计数单例：

- 第一台设备进入时调用 SDK 的 `InitSdk()`。
- 最后一台设备释放时调用 `ShutdownSdk()`。
- 用单例 + 引用计数管理；不要让上层关心。

无全局初始化要求的 SDK 可以省略这一层。

## 5. 单机统一封装：`CameraDevice`

定义在 [camera_device.h](../include/camera/core/camera_device.h)。

### 5.1 职责

1. 接管 `ICameraSdkAdapter` 的所有权（`unique_ptr` / Python 中持有引用即可）。
2. 缓存当前 `CameraConfig` 与 `CameraRuntimeState`。
3. 实现一个明确的状态机：
   `Created → Discovered → Opened → Configured → Streaming ↔ Stopped`，任意点出错 → `Error`。
4. 把 adapter 的 `FrameCallback` 适配成对外的 `FrameHandler`，并在转发前更新统计：
   - `totalFrames += 1`
   - `lastFrameTime = now()`
   - `lastFrameLatencyUs = now() - frame.meta.timestamp`
   - `maxFrameLatencyUs = max(maxFrameLatencyUs, lastFrameLatencyUs)`
5. 用一把互斥锁（C++ `std::mutex`，Python `threading.Lock`）保护 `config_ / runtimeState_ / frameHandler_`。

### 5.2 公开接口

```
info() / runtimeState() / currentConfig()
open() / close()
applyConfig(config)
start() / stop()
executeSoftwareTrigger()
setFrameHandler(handler)
```

### 5.3 私有逻辑

- `updateState(newState, lastError = "")`：状态机唯一入口，集中加锁。
- `onFrame(frame)`：内部回调入口，更新统计 → 转发给 `frameHandler_`。

## 6. 多机管理：`CameraManager`

定义在 [camera_manager.h](../include/camera/core/camera_manager.h)。

**只管多台设备的发现与查询，不做任何业务编排**（业务编排放 `CameraService`）。

| 方法 | 行为 |
|---|---|
| `CameraManager(factory)` | 持有工厂，但不立即枚举 |
| `discover()` | factory 枚举 → 对每台设备 `createAdapter` → 包成 `CameraDevice` → 用 `map[serialNumber, CameraDevice]` 缓存（**重建**而非增量） |
| `listDiscovered()` | 返回最近一次扫描到的 `CameraInfo` 列表 |
| `getCameraBySerialNumber(sn)` | 按序列号查具体设备，找不到返回空 |
| `listCameras()` | 返回所有 `CameraDevice` 引用列表 |
| `openAll() / closeAll()` | 顺序遍历，一台失败立即返回 |

**约束**：相机处于 `Opened / Configured / Streaming` 时不能重复 `discover()`，当前实现会拒绝。

## 7. 帧分发：`CapturePipeline` + `IFrameSink` + `FrameSubscription`

定义在 [camera_pipeline.h](../include/camera/core/camera_pipeline.h)。一条克制的“发布-订阅”管道。

### 7.1 `IFrameSink`

```
class IFrameSink:
    consume(frame: FramePtr) -> None
```

所有帧消费者（存图、拼图、检测、调试推流）都实现这个接口。

### 7.2 `FrameSubscription`

描述一个 sink 关注哪些帧。所有**已填写**字段以 AND 关系匹配；**全空 = 收所有帧**。

| 字段 | 匹配规则 |
|---|---|
| `cameraSerialNumbers` | 精确匹配 `frame.meta.cameraSerialNumber` |
| `cameraNames` | 精确匹配 `frame.meta.cameraName` |
| `taskIds` | 精确匹配 `frame.meta.taskId` |
| `onlyHighExposure` | 为 true 时仅接收 `frame.meta.highExposure == true` |

### 7.3 `CapturePipeline`

| 方法 | 行为 |
|---|---|
| `addSink(sink, subscription = empty)` | 注册 sink；空订阅 = 收所有帧 |
| `removeAllSinks()` | 清空所有 sink |
| `publish(frame)` | **同步**遍历 sink，匹配则调 `consume(frame)` |
| `stats()` | 返回 `CapturePipelineStats` 快照 |
| `resetStats()` | 清零统计，便于阶段性观测 |

`CapturePipelineStats` 字段：
`publishedFrames / deliveredFrames / emptyPublishCount / sinkCount / totalPublishNanos / maxPublishNanos / slowPublishCount / lastPublishTime`。

**关键约束**：`publish()` 是同步的，`consume()` 跑在调用线程（即 SDK 回调线程）。**sink 必须快速返回**，重活（写盘、编码、网络）由 sink 内部异步化。参考 `SaveImageConsumer` 的“有界队列 + 满则丢帧、不反压采集线程”模式。

## 8. 统一入口：`CameraService`

定义在 [camera_service.h](../include/camera/core/camera_service.h)。**业务层唯一应该依赖的类。**

内部组合：`CameraManager` + `CapturePipeline`。

### 8.1 生命周期接口

| 方法 | 语义 |
|---|---|
| `discover()` | 扫描设备；自动通过 `bindFrameHandlers()` 把每台 device 的输出帧接进 pipeline |
| `initialize()` | 当前 = 打开全部相机；保留语义未来可扩展统一资源初始化 |
| `applyCameraConfigs(map[sn, CameraConfig])` | 按序列号批量下发 |
| `startCamerasBySerialNumbers(sns)` / `stopCamerasBySerialNumbers(sns)` | 选择性启停 |
| `executeSoftwareTriggerBySerialNumbers(sns)` | 选择性软触发 |
| `shutdown()` | 当前 = 关闭全部相机 |

### 8.2 查询与观测

| 方法 | 返回 |
|---|---|
| `cameras()` | `vector<CameraInfo>` |
| `getCameraBySerialNumber(sn)` | `CameraDevice` 或空 |
| `cameraRuntimeStates()` | `map[sn, CameraRuntimeState]` |
| `pipelineStats()` | `CapturePipelineStats` |
| `resetPipelineStats()` | 清零 |

### 8.3 Sink 管理

| 方法 | 行为 |
|---|---|
| `addFrameSink(sink, subscription = empty)` | 注册到内部 pipeline |
| `clearFrameSinks()` | 清空 |

### 8.4 标准调用顺序

```
1.  ICameraSdkFactory 构造
2.  CameraService(factory) 构造
3.  service.discover()
4.  service.initialize()              # = openAll
5.  service.applyCameraConfigs(...)
6.  service.addFrameSink(...)         # 可选
7.  service.startCamerasBySerialNumbers([...])
8.  service.executeSoftwareTriggerBySerialNumbers([...])   # 软触发模式时
9.  service.stopCamerasBySerialNumbers([...])
10. service.shutdown()
```

## 9. 关键设计决策

| 决策 | 原因 |
|---|---|
| 用序列号而不是 logicalName 当 key | logicalName 可能因设备重排错绑；序列号是设备稳定标识 |
| 帧用 `shared_ptr<const FramePacket>` 传递 | 多 sink 共享同一帧；只读保证语义安全 |
| `publish()` 同步分发 | 简单可控；耗时逻辑下放给 sink 自己异步 |
| 状态机集中在 `CameraDevice.updateState` | 单点更新，便于加锁和排障 |
| `CameraManager.discover()` 重建而非增量 | 避免“老对象残留 + 新对象重复”的难调试问题 |
| 单独抽 `ICameraSdkFactory` 而不是把枚举塞到 adapter | 枚举是 SDK 的全局行为，不是单机行为 |
| SDK Runtime 用引用计数单例 | 厂商 SDK 普遍要求“最早 init，最晚 shutdown” |
| 错误用 `bool + errorMessage` 双返回 | C++ 端避免异常跨 SDK 边界；Python 端可改为抛 `CameraError` |

## 10. Python 移植说明

整套类设计在 Python 端按相同的拓扑直接复刻即可，只在几处做语言层适配。

### 10.1 直接对应

| C++ | Python |
|---|---|
| `enum class` | `enum.Enum` |
| `struct` | `@dataclass`（`FramePacket` 不要 `frozen`，`data` 字段可能很大）|
| 接口（纯虚类）| `abc.ABC` + `@abstractmethod` |
| `std::shared_ptr<const FramePacket>` | `FramePtr = FramePacket` 类型别名 + 上游不修改约定 |
| `std::mutex` | `threading.Lock` |
| `std::unordered_map` | `dict` |

### 10.2 需要调整的几处

1. **错误处理风格**：把 `bool + string* errorMessage` 改成抛 `CameraError`（自定义异常）。HTTP 边界统一 `try/except` 把异常翻译成 `{"ok": false, "error": "..."}` 即可。
2. **线程模型**：SDK 回调通常跑在 SDK 内部线程；Python 里要小心 GIL，回调内务必入队（`queue.Queue` + worker thread）。`SaveImageConsumer` 的“快速入队、满则丢帧、不反压采集线程”模式是参考样板。
3. **帧数据载体**：`FramePacket.data` 推荐用 `bytes` 或 `numpy.ndarray`；如果上层需要 OpenCV，统一约定用 `BGR8 + numpy.ndarray` 直接交给 `cv2`。
4. **统计字段类型**：C++ 用 `atomic`；Python 直接 `int + Lock` 即可。

### 10.3 建议目录结构

```
camera/
  core/
    types.py          # CameraRole / CameraState / TriggerMode / PixelFormat / CameraInfo / ...
    sdk_adapter.py    # ICameraSdkAdapter / ICameraSdkFactory（ABC）
    device.py         # CameraDevice
    manager.py        # CameraManager
    pipeline.py       # CapturePipeline / IFrameSink / FrameSubscription / Stats
    service.py        # CameraService
  sinks/
    save_image_consumer.py
  filecamera/         # 假相机：先用这个跑通流程
    file_image_adapter.py
    file_image_factory.py
  ikap/               # 真厂商：ctypes / pybind 绑 SDK
    ikap_runtime.py
    ikap_adapter.py
    ikap_factory.py
```

### 10.4 接入新厂商的最小 checklist

与 C++ 端 [camera_vendor_extension_guide.md](camera_vendor_extension_guide.md) 完全一致：

1. 先写 `FooCameraSdkFactory`
2. 再写 `CameraFooSdkAdapter`
3. 打通 `enumerate_devices()`
4. 打通 `open() / close()`
5. 打通 `apply_config()`
6. 打通 `start_grabbing() / stop_grabbing()`
7. 最后补 `execute_software_trigger()`
8. 补测试和日志

**强烈建议**：Python 端也先实现 `FileImageCameraSdkFactory`（读目录里的图片定时回放），跑通 `CameraService` 主链路后再接真 SDK。这是 C++ 端被验证过的最快接入路径。

## 11. 相关文件

- [camera_types.h](../include/camera/core/camera_types.h)
- [camera_sdk_adapter.h](../include/camera/core/camera_sdk_adapter.h)
- [camera_device.h](../include/camera/core/camera_device.h)
- [camera_manager.h](../include/camera/core/camera_manager.h)
- [camera_pipeline.h](../include/camera/core/camera_pipeline.h)
- [camera_service.h](../include/camera/core/camera_service.h)
- [camera_core_usage.md](camera_core_usage.md)
- [camera_vendor_extension_guide.md](camera_vendor_extension_guide.md)
