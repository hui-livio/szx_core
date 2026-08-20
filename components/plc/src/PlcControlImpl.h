/**
 * @file PlcControlImpl.h
 * @brief PLC 控制实现类 — 封装 Modbus TCP 读写和状态查询。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "PlcControl.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "BlockQueue.hpp"
#include "modbus.h"


namespace SZXPLC
{

using SteadyClock = std::chrono::steady_clock;
using TimePoint   = SteadyClock::time_point;

namespace
{
constexpr const char LOG_TAG[] = "PLC";
}

struct ModbusDeleter
{
    void operator()(modbus_t *ctx) const
    {
        if (ctx)
        {
            modbus_close(ctx);
            modbus_free(ctx);
        }
    }
};

inline uint16_t requiredRegisterCountForDataType(PollDataType dataType)
{
    switch (dataType)
    {
    case PollDataType::U32:
    case PollDataType::S32:
    case PollDataType::F32:
        return 2;

    case PollDataType::Bytes:
    case PollDataType::U16:
    case PollDataType::S16:
    default:
        return 1;
    }
}

// ========================================================================
// 以下结构体仅用于 DLL 内部，不暴露给外部调用方
// ========================================================================

struct ModbusResult
{
    RegisterType type     = RegisterType::HoldingRegisters;
    uint16_t startAddress = 0;
    uint16_t count        = 0;
    std::vector<uint16_t> values;
};

struct ReadJob
{
    RegisterType type = RegisterType::HoldingRegisters;
    uint16_t address  = 0;
    uint16_t count    = 1;
    int unitId        = 1;
    std::string tag   = "read";
    int attempt       = 0;
    std::function<void(bool, const ModbusResult &, const std::string &)> onDone;
};

struct WriteJob
{
    RegisterType type = RegisterType::HoldingRegisters;
    uint16_t address  = 0;
    std::vector<uint16_t> values;
    int unitId      = 1;
    std::string tag = "write";
    int attempt     = 0;
    std::function<void(bool, const std::string &)> onDone;
};

struct PollItem
{
    PollId id             = 0;
    int unitId            = 1;
    RegisterType type     = RegisterType::HoldingRegisters;
    uint16_t address      = 0;
    uint16_t count        = 1;
    int intervalMs        = 1000;
    bool onChangeOnly     = true;
    PollDataType dataType = PollDataType::U16;
    bool wordSwap         = false;
    double deadband       = 0.0;
    std::string tag       = "poll";
    TimePoint nextDue{};
    bool inFlight = false;
    std::vector<uint8_t> lastRaw;
};

// ========================================================================
// PLCController::Impl
// ========================================================================

struct PLCController::Impl
{
    // Connection params
    std::string ip_;
    uint16_t port_{502};
    int unitId_{1};

    // Modbus
    std::unique_ptr<modbus_t, ModbusDeleter> ctx_;
    mutable std::mutex ctxMutex_;

    // Worker thread
    std::thread worker_;
    std::atomic<bool> workerRunning_{false};
    std::atomic<ConnState> state_{ConnState::Disconnected};

    // Queues
    SZX::COMMON::BlockingQueue<ReadJob> readQueue_;
    SZX::COMMON::BlockingQueue<WriteJob> writeQueue_;

    // Config
    int readGapMs_{50};           // read gap in ms
    int writeGapMs_{25};          // write gap in ms
    int retries_{1};              // max retries
    bool multiplyAddrBy2_{false}; // multiply address by 2
    bool defaultWordSwap_{false}; // default word swap

    // Reconnect
    int consecutiveErrors_{0};      // consecutive errors count
    int maxConsecutiveErrors_{5};   // max consecutive errors count
    int reconnectDelayMs_{500};     // reconnect delay in ms
    int maxReconnectDelayMs_{5000}; // max reconnect delay in ms

    // Poll
    std::mutex pollMutex_;
    std::unordered_map<PollId, PollItem> pollItems_;
    std::atomic<bool> pollSchedulerRunning_{false};
    int pollTickMs_{50};
    int maxPollsPerTick_{1};
    bool skipIfInFlight_{true};
    bool pollLogging_{false};
    std::atomic<uint64_t> nextPollId_{1};

    // Heartbeat
    HeartbeatConfig heartbeat_;
    int hbFailCount_{0};
    uint16_t lastHbValue_{0};
    bool hasLastHbValue_{false};
    TimePoint lastHbTime_{};

    // Callbacks
    mutable std::mutex cbMutex_;
    IPLCControllerCallbacks *callbacks_ = nullptr;

    // --- Methods ---

    void setState(ConnState s);
    bool createAndConnectContext();
    bool doRead(const ReadJob &job, ModbusResult &out, std::string &err);
    bool doWrite(const WriteJob &job, std::string &err);
    void ioLoop();
    bool handleHeartbeatIfDue();
    void processPollSchedule();
    void processPollCallback(PollId id, bool success, const ModbusResult &result, const std::string &error);
    static uint16_t mapAddr_(uint16_t addr, bool mul2);
    static std::vector<uint8_t> resultToRaw(const ModbusResult &result);
    static double extractNumericValue(const std::vector<uint8_t> &raw, PollDataType dataType, bool wordSwap);
    bool isPollValueChanged(const PollItem &item, const std::vector<uint8_t> &newRaw);
    void notifyConnected();
    void notifyConnectionFailed(const std::string &err);
    void notifyConnectionLost();
    void clearWriteQueue();
    void startWorkerIfNeeded();
    void stopWorker();
    void startPollScheduler();
    void stopPollScheduler();
    void enqueueRead(const ReadJob &job);
    void enqueueWrite(const WriteJob &job);
    bool reconnect();
};

} // namespace SZXPLC
