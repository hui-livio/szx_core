# plc_core 使用说明

`plc_core` 提供基于 `libmodbus` 的 PLC 控制能力，目标是把连接、读写、轮询、心跳、自动重连这些通用逻辑集中在 `PLCController` 里，业务层尽量只面向统一接口。

## 1. 模块职责

核心入口是 `SZXPLC::PLCController`，它负责：

- 建立和关闭 PLC TCP 连接
- 异步读写寄存器
- 周期性轮询
- 心跳监测
- 连接状态回调
- 自动重连

## 2. 关键类型

主要定义在 `include/plc/plccontrol.h`。

- `ConnState`
  连接状态：`Disconnected`、`Connecting`、`Connected`、`Error`、`Reconnecting`
- `RegisterType`
  寄存器类型：`Coils`、`DiscreteInputs`、`HoldingRegisters`、`InputRegisters`
- `PollDataType`
  轮询数据解释类型：`Bytes`、`U16`、`S16`、`U32`、`S32`、`F32`
- `ModbusResult`
  一次读操作的结果，包含地址、数量、返回值
- `PollOptions`
  配置轮询行为，比如 `unitId`、`dataType`、`deadband`、`wordSwap`
- `Callbacks`
  所有连接和轮询相关回调

## 3. 标准生命周期

推荐调用顺序：

1. 创建 `PLCController`
2. 配置参数和回调
3. `connectToPLC(ip, port)`
4. 执行读写 / 轮询 / 心跳
5. `disconnectPLC()`

说明：

- `connectToPLC()` 成功后会启动内部工作线程。
- `disconnectPLC()` 会停止轮询、停止工作线程、清空读写队列并关闭连接。

## 4. 最小连接示例

```cpp
#include "plc/plccontrol.h"

#include <iostream>
#include <string>

int main()
{
    SZXPLC::PLCController plc;

    SZXPLC::Callbacks callbacks;
    callbacks.onConnected = []() {
        std::cout << "PLC connected" << std::endl;
    };
    callbacks.onConnectionFailed = [](const std::string &err) {
        std::cout << "PLC connect failed: " << err << std::endl;
    };
    callbacks.onConnectionLost = []() {
        std::cout << "PLC connection lost" << std::endl;
    };
    callbacks.onStateChanged = [](SZXPLC::ConnState state) {
        std::cout << "state=" << static_cast<int>(state) << std::endl;
    };

    plc.setCallbacks(callbacks);
    plc.setUnitId(1);

    if (!plc.connectToPLC("127.0.0.1", 502)) {
        std::cout << "connectToPLC returned false" << std::endl;
        return 1;
    }

    // 业务操作...

    plc.disconnectPLC();
    return 0;
}
```

## 5. 写寄存器示例

### 写 16 位整数

```cpp
plc.writeInt(0, 123);
```

### 写 float

```cpp
plc.writeFloat(2, 3.14f, false);
```

### 写多个寄存器

```cpp
plc.writeRegisters(10, {1, 2, 3, 4});
```

### 写 float 数组

```cpp
plc.writeFloatArray(20, {1.5f, 2.5f, 3.5f}, false);
```

说明：

- `writeFloat()` / `writeFloatArray()` 内部会把 `float` 打包成两个 16 位寄存器。
- `wordSwap` 是否需要开启，取决于 PLC 和寄存器协议约定。

## 6. 读寄存器示例

```cpp
plc.readRegisters(0, 4);
```

当前 `readRegisters()` 是异步入队行为，实际结果通常通过回调链路或轮询机制获取。

## 7. 轮询示例

`PLCController` 支持定时轮询寄存器，并在数据变化或出错时回调。

```cpp
SZXPLC::Callbacks callbacks;
callbacks.onPollChanged = [](SZXPLC::PollId id, float value, SZXPLC::TimestampMs ts) {
    std::cout << "poll changed, id=" << id << ", value=" << value << ", ts=" << ts << std::endl;
};
callbacks.onPollError = [](SZXPLC::PollId id, const std::string &err) {
    std::cout << "poll error, id=" << id << ", err=" << err << std::endl;
};
plc.setCallbacks(callbacks);

SZXPLC::PollOptions opt;
opt.unitId = 1;
opt.type = SZXPLC::RegisterType::HoldingRegisters;
opt.dataType = SZXPLC::PollDataType::F32;
opt.deadband = 0.5;
opt.wordSwap = false;
opt.onChangeOnly = true;
opt.tag = "height_poll";

const auto pollId = plc.addPoll(0, 2, 100, opt);
```

说明：

- `intervalMs` 是轮询周期。
- `count` 应与数据类型匹配。比如 `F32/U32/S32` 通常至少需要 2 个寄存器。
- `onChangeOnly=true` 时，只有变化超过阈值才触发 `onPollChanged`。
- `deadband` 对浮点等连续值很有用。

更新轮询：

```cpp
plc.updatePoll(pollId, 200, std::nullopt);
```

删除轮询：

```cpp
plc.removePoll(pollId);
```

清空全部轮询：

```cpp
plc.clearPolls();
```

## 8. 心跳示例

可以通过周期读取某个寄存器来判断 PLC 是否存活。

```cpp
plc.enableHeartbeat(
    0,      // 心跳寄存器地址
    1000,   // 周期 ms
    3,      // 连续失败阈值
    false   // 是否要求数值变化
);
```

关闭心跳：

```cpp
plc.disableHeartbeat();
```

说明：

- 如果连续多次读取失败，连接状态可能进入 `Error`，随后内部会尝试重连。
- `requireValueChange=true` 时，如果心跳寄存器一直不变，也会累计失败次数。

## 9. 地址与字节序注意事项

### 地址倍增

某些 PLC 地址文档是按字节或特定偏移体系给出的，此时可以开启：

```cpp
plc.setAddressMultiplyBy2(true);
```

开启后，内部会把传入地址乘 2 再访问。

### 字序交换

如果 PLC 对 32 位数值采用不同 word 排列，可以设置：

```cpp
plc.setDefaultWordSwap(true);
```

或者在写 float 时单独指定：

```cpp
plc.writeFloat(2, 3.14f, true);
```

## 10. 常用配置项

```cpp
plc.setUnitId(1);
plc.setReadGapMs(50);
plc.setWriteGapMs(25);
plc.setRetries(1);
plc.setPollTickMs(50);
plc.setMaxPollsPerTick(1);
plc.setSkipIfInFlight(true);
plc.setPollLogging(false);
```

常见用途：

- `setReadGapMs` / `setWriteGapMs`
  控制连续读写的节流间隔。
- `setRetries`
  失败后的重试次数。
- `setPollTickMs`
  轮询调度器 tick 间隔。
- `setMaxPollsPerTick`
  每个 tick 最多触发多少个 poll，避免瞬时打爆链路。

## 11. 自动重连行为

内部在出现严重错误时会：

- 进入 `ConnState::Error`
- 清理写队列
- 触发 `onConnectionLost`
- 进入 `Reconnecting`
- 按退避策略尝试重连

这意味着业务层通常不需要自己再包一层重连线程，但需要关注：

- 重连期间是否允许继续写入
- 是否需要在 `onConnected` 后重新下发初始化参数

## 12. 常见注意事项

- `writeInt()` 的参数类型是 `int16_t`，超范围会截断。
- `F32/U32/S32` 轮询时建议 `count >= 2`。
- `readRegisters()` / `write*()` 当前是异步队列模式，不是阻塞直到 PLC 返回。
- `libmodbus` 的地址基准、功能码和 PLC 文档可能并不完全一致，接入前需要先做地址校对。
- 如果上层只关心“变化事件”，优先用 `onPollChanged`，比自己高频读寄存器更省事。

## 13. 相关文件

- `include/plc/plccontrol.h`
- `src/plc/plccontrol.cpp`
- `tests/test_plc_controller.cpp`
