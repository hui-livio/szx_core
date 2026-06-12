#pragma once
#include "plc/plc_core_api.h"
#include "common/queue/blocking_queue.h"
#include "uuid.h"
#include "modbus.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SZXPLC
{
    using PollId = uint64_t;
    using SteadyClock = std::chrono::steady_clock;
    using SystemClock = std::chrono::system_clock;
    using TimePoint = SteadyClock::time_point;
    using TimestampMs = int64_t;

    enum class RegisterType {
        Coils,
        DiscreteInputs,
        HoldingRegisters,
        InputRegisters
    };

    enum class PollDataType {
        Bytes,
        U16,
        S16,
        U32,
        S32,
        F32
    };

    enum class ConnState {
        Disconnected,
        Connecting,
        Connected,
        Error,
        Reconnecting
    };

    struct ModbusCtxDeleter {
        void operator()(modbus_t* ctx) const {
            if (ctx) {
                modbus_close(ctx);
                modbus_free(ctx);
            }
        }
    };

    using ModbusCtxPtr = std::unique_ptr<modbus_t, ModbusCtxDeleter>;

    struct PLC_CORE_API ModbusResult {
        RegisterType type = RegisterType::HoldingRegisters;
        uint16_t startAddress = 0;
        uint16_t count = 0;
        std::vector<uint16_t> values;
    };

    struct PLC_CORE_API ReadJob {
        RegisterType type = RegisterType::HoldingRegisters;
        uint16_t address = 0;
        uint16_t count = 1;
        int unitId = 1;
        std::string tag = "read";
        int attempt = 0;
        std::function<void(bool, const ModbusResult&, const std::string&)> onDone;
    };

    struct PLC_CORE_API WriteJob {
        RegisterType type = RegisterType::HoldingRegisters;
        uint16_t address = 0;
        std::vector<uint16_t> values;
        int unitId = 1;
        std::string tag = "write";
        int attempt = 0;
        std::function<void(bool, const std::string&)> onDone;
    };

    struct PLC_CORE_API PollOptions {
        int unitId = 1;
        RegisterType type = RegisterType::HoldingRegisters;
        bool onChangeOnly = true;
        PollDataType dataType = PollDataType::U16;
        bool wordSwap = false;
        double deadband = 0.0;
        int timeoutMs = 1000; // 预留
        std::string tag = "poll";
    };

    struct PLC_CORE_API PollItem {
        PollId id = 0;
        int unitId = 1;
        RegisterType type = RegisterType::HoldingRegisters;
        uint16_t address = 0;
        uint16_t count = 1;
        int intervalMs = 1000;
        bool onChangeOnly = true;
        PollDataType dataType = PollDataType::U16;
        bool wordSwap = false;
        double deadband = 0.0;
        std::string tag = "poll";
        TimePoint nextDue{};
        bool inFlight = false;
        std::vector<uint8_t> lastRaw;
    };

    struct PLC_CORE_API HeartbeatConfig {
        bool enabled = false;
        uint16_t address = 0;
        int intervalMs = 1000;
        int failThreshold = 3;
        bool requireValueChange = false;
    };

    struct PLC_CORE_API Callbacks {
        std::function<void()> onConnected;
        std::function<void(const std::string&)> onConnectionFailed;
        std::function<void()> onConnectionLost;
        std::function<void(ConnState)> onStateChanged;

        std::function<void(PollId, const std::vector<uint8_t>&, TimestampMs)> onPollValue;
        std::function<void(PollId, float, TimestampMs)> onPollChanged;
        std::function<void(PollId, const std::string&)> onPollError;
    };

    class PLC_CORE_API PLCController {
        public:
             PLCController();
            ~PLCController();

            PLCController(const PLCController&) = delete;
            PLCController& operator=(const PLCController&) = delete;

            // 生命周期
            bool connectToPLC(const std::string& ip, uint16_t port);
            void disconnectPLC();

            // 配置
            void setUnitId(int uid) { unitId_ = uid; }
            void setReadGapMs(int ms) { readGapMs_ = ms < 0 ? 0 : ms; }
            void setWriteGapMs(int ms) { writeGapMs_ = ms < 0 ? 0 : ms; }
            void setRetries(int n) { retries_ = n < 0 ? 0 : n; }
            void setAddressMultiplyBy2(bool v) { multiplyAddrBy2_ = v; }
            void setDefaultWordSwap(bool v) { defaultWordSwap_ = v; }
            void setPollTickMs(int ms) { pollTickMs_ = ms < 10 ? 10 : ms; }
            void setMaxPollsPerTick(int n) { maxPollsPerTick_ = n < 1 ? 1 : n; }
            void setSkipIfInFlight(bool on) { skipIfInFlight_ = on; }
            void setPollLogging(bool on) { pollLogging_ = on; }

            // 回调
            void setCallbacks(Callbacks callbacks);

            // 状态
            bool isRunning() const { return state_.load() == ConnState::Connected; }
            ConnState state() const { return state_.load(); }

            // 读写
            void writeInt(uint16_t address, int16_t data);
            void writeFloat(uint16_t address, float data, bool wordSwap = false);
            void writeRegisters(uint16_t startAddress0, const std::vector<uint16_t>& regs);
            void writeFloatArray(uint16_t startAddress0, const std::vector<float>& values, bool wordSwap = false);
            void readRegisters(uint16_t address, uint16_t registerCount);

            // 轮询
            PollId addPoll(uint16_t address, uint16_t count, int intervalMs, const PollOptions& opt = {});
            bool updatePoll(PollId id, std::optional<int> intervalMs = {}, std::optional<PollOptions> opt = {});
            bool removePoll(PollId id);
            void clearPolls();
            void startPollScheduler();
            void stopPollScheduler();

            // 心跳
            void enableHeartbeat(uint16_t address, int intervalMs = 1000, int failThreshold = 3, bool requireValueChange = false);
            void disableHeartbeat();

            // 工具
            static std::pair<uint16_t, uint16_t> packFloatToRegs(float v, bool wordSwap);
            static std::vector<uint8_t> floatToBytes(float data);

        private:
            static uint16_t mapAddr_(uint16_t addr, bool mul2);
            static TimestampMs nowSystemMs();

            void setState(ConnState s);

            bool createAndConnectContext();
            bool reconnect();

            void startWorkerIfNeeded();
            void stopWorker();

            void enqueueRead(const ReadJob& job);
            void enqueueWrite(const WriteJob& job);

            void ioLoop();

            bool doRead(const ReadJob& job, ModbusResult& out, std::string& err);
            bool doWrite(const WriteJob& job, std::string& err);

            bool handleHeartbeatIfDue();

            void processPollSchedule();
            void processPollCallback(PollId id, bool success, const ModbusResult& result, const std::string& error);

            static std::vector<uint8_t> resultToRaw(const ModbusResult& result);
            static double extractNumericValue(const std::vector<uint8_t>& raw, PollDataType dataType, bool wordSwap);
            static bool isPollValueChanged(const PollItem& item, const std::vector<uint8_t>& newRaw);

            void clearWriteQueue();
            void notifyConnected();
            void notifyConnectionFailed(const std::string& err);
            void notifyConnectionLost();

        private:
            // 连接参数
            std::string ip_;
            uint16_t port_ = 502;
            int unitId_ = 1;

            // modbus
            ModbusCtxPtr ctx_;
            mutable std::mutex ctxMutex_;

            // 工作线程
            std::thread worker_;
            std::atomic<bool> workerRunning_{false};
            std::atomic<ConnState> state_{ConnState::Disconnected};

            // 队列
            SZXCamera::BlockingQueue<ReadJob> readQueue_;
            SZXCamera::BlockingQueue<WriteJob> writeQueue_;

            // 配置
            int readGapMs_ = 50;
            int writeGapMs_ = 25;
            int retries_ = 1;
            bool multiplyAddrBy2_ = false;
            bool defaultWordSwap_ = false;

            // 自动重连
            int consecutiveErrors_ = 0;
            int maxConsecutiveErrors_ = 5;
            int reconnectDelayMs_ = 500;
            int maxReconnectDelayMs_ = 5000;

            // 轮询
            std::mutex pollMutex_;
            std::unordered_map<PollId, PollItem> pollItems_;
            std::atomic<bool> pollSchedulerRunning_{false};
            int pollTickMs_ = 50;
            int maxPollsPerTick_ = 1;
            bool skipIfInFlight_ = true;
            bool pollLogging_ = false;
            std::atomic<uint64_t> nextPollId_{1};

            // 心跳
            HeartbeatConfig heartbeat_;
            int hbFailCount_ = 0;
            uint16_t lastHbValue_ = 0;
            bool hasLastHbValue_ = false;
            TimePoint lastHbTime_{};

            // 回调
            mutable std::mutex cbMutex_;
            Callbacks callbacks_;
    };
}





