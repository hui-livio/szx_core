/**
 * @file PlcControl.h
 * @brief PLC 控制接口 — 定义跨 DLL 使用的 PLC 客户端抽象。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include <cstdint>
#include <memory>


// plc 模块仅支持动态库，不允许静态库和源码编译

#if defined(_WIN32) || defined(WIN64)
    #ifdef AOI_PLC_EXPORTS
        #define AOI_PLC_API __declspec(dllexport)
    #else
        #define AOI_PLC_API __declspec(dllimport)
    #endif
#else
    #define AOI_PLC_API __attribute__((visibility("default")))
#endif


namespace SZXPLC
{
using PollId      = uint64_t;
using TimestampMs = int64_t;

enum class RegisterType : uint8_t
{
    Coils,            // 线圈
    DiscreteInputs,   // 离散输入
    HoldingRegisters, // 保持寄存器
    InputRegisters    // 输入寄存器
};

enum class PollDataType : uint8_t
{
    Bytes,
    U16,
    S16,
    U32,
    S32,
    F32
};

enum class ConnState : uint8_t
{
    Disconnected,
    Connecting,
    Connected,
    Error,
    Reconnecting
};

// ========================================================================
// 跨 DLL 安全的公共结构体（仅含 POD 类型）
// ========================================================================

struct PollOptions
{
    int unitId            = 1;
    RegisterType type     = RegisterType::HoldingRegisters;
    bool onChangeOnly     = true;
    PollDataType dataType = PollDataType::U16;
    bool wordSwap         = false;
    double deadband       = 0.0;
    int timeoutMs         = 1000;
    const char *tag       = nullptr; // 可选，调用方持有，内部立即拷贝
};

struct HeartbeatConfig
{
    bool enabled            = false;
    uint16_t address        = 0;
    int intervalMs          = 1000;
    int failThreshold       = 3;
    bool requireValueChange = false;
};

struct AOI_PLC_API IPLCControllerCallbacks
{
    virtual ~IPLCControllerCallbacks() = default;

    virtual void onConnected()                                                        = 0;
    virtual void onConnectionFailed(const char *error)                                = 0;
    virtual void onConnectionLost()                                                   = 0;
    virtual void onStateChanged(ConnState state)                                      = 0;
    virtual void onPollValue(PollId id, const uint8_t *data, int len, TimestampMs ts) = 0;
    virtual void onPollChanged(PollId id, float value, TimestampMs ts)                = 0;
    virtual void onPollError(PollId id, const char *error)                            = 0;
};

// ========================================================================
// PLCController
// ========================================================================

class AOI_PLC_API PLCController
{
public:
    PLCController();
    ~PLCController();

    // disabled copy constructor and assignment operator
    PLCController(const PLCController &)            = delete;
    PLCController &operator=(const PLCController &) = delete;

    // 生命周期
    bool connectToPLC(const char *ip, uint16_t port);
    void disconnectPLC();

    // 配置
    void setUnitId(int uid);
    void setReadGapMs(int ms);
    void setWriteGapMs(int ms);
    void setRetries(int n);
    void setAddressMultiplyBy2(bool v);
    void setDefaultWordSwap(bool v);
    void setPollTickMs(int ms);
    void setMaxPollsPerTick(int n);
    void setSkipIfInFlight(bool on);
    void setPollLogging(bool on);

    // 回调
    void setCallbacks(IPLCControllerCallbacks *callbacks);

    // 状态
    bool isRunning() const;
    ConnState state() const;

    // 异步读写（注：指针参数由调用方持有，内部立即拷贝）
    void writeInt(uint16_t address, int16_t data);
    void writeFloat(uint16_t address, float data, bool wordSwap = false);
    void writeRegisters(uint16_t startAddress, const uint16_t *regs, size_t count);
    void writeFloatArray(uint16_t startAddress, const float *values, size_t count, bool wordSwap = false);
    void readRegisters(uint16_t address, uint16_t registerCount);

    // 同步读写（直接调 libmodbus，调用线程阻塞, 无超时, 直接返回结果 ）
    bool readRegistersSync(uint16_t address, uint16_t count, uint16_t *outValues);
    bool writeRegisterSync(uint16_t address, uint16_t value);
    bool readCoilsSync(uint16_t address, uint16_t count, uint8_t *outBits);
    bool writeCoilSync(uint16_t address, bool value);

    // 轮询（指针为 nullptr 表示不修改对应参数）
    PollId addPoll(uint16_t address, uint16_t count, int intervalMs, const PollOptions *opt = nullptr);
    bool updatePoll(PollId id, const int *intervalMs = nullptr, const PollOptions *opt = nullptr);
    bool removePoll(PollId id);
    void clearPolls();
    void startPollScheduler();
    void stopPollScheduler();

    // 心跳
    void enableHeartbeat(uint16_t address, int intervalMs = 1000, int failThreshold = 3, bool requireValueChange = false);
    void disableHeartbeat();

    // 工具（出参方式，避免返回 STL 容器跨越 DLL 边界）
    static void packFloatToRegs(float v, bool wordSwap, uint16_t &outLow, uint16_t &outHigh);
    static void floatToBytes(float data, uint8_t outBytes[4]);

private:
    struct Impl;

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251) // C4251: STL 成员在导出类中需要 dll 接口——PIMPL 惯用法固有现象，可安全忽略
#endif
    std::unique_ptr<Impl> impl_;
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
};

} // namespace SZXPLC
