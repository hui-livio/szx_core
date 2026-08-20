/**
 * @file PlcControlImpl.cc
 * @brief PLC 控制实现 — 管理连接、寄存器读写和错误处理。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "PlcControlImpl.h"

#include <cmath>
#include "modbus.h"

#include "Logger.h"
#include "MagicEnum.hpp"
#include "TimeHelper.hpp"

using namespace SZX::LOGGER;
using namespace SZX::COMMON;

namespace SZXPLC
{

// ========================================================================
// Impl static / utility methods
// ========================================================================

uint16_t PLCController::Impl::mapAddr_(uint16_t addr, bool mul2)
{
    return mul2 ? static_cast<uint16_t>(addr * 2) : addr;
}

std::vector<uint8_t> PLCController::Impl::resultToRaw(const ModbusResult &result)
{
    std::vector<uint8_t> raw(result.count * 2);
    for (size_t i = 0; i < result.values.size(); ++i)
    {
        uint16_t v     = result.values[i];
        raw[i * 2]     = static_cast<uint8_t>(v & 0xFF);
        raw[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }
    return raw;
}

double PLCController::Impl::extractNumericValue(const std::vector<uint8_t> &raw, PollDataType dataType, bool wordSwap)
{
    auto u8 = [](uint8_t c) -> uint16_t
    {
        return static_cast<uint16_t>(c);
    };

    if (raw.size() < 2)
    {
        return 0.0;
    }

    switch (dataType)
    {
    case PollDataType::Bytes:
        return 0.0;

    case PollDataType::U16:
    {
        uint16_t b0 = u8(raw[0]);
        uint16_t b1 = u8(raw[1]);
        uint16_t v  = static_cast<uint16_t>((b1 << 8) | b0);
        return static_cast<double>(v);
    }

    case PollDataType::S16:
    {
        uint16_t b0 = u8(raw[0]);
        uint16_t b1 = u8(raw[1]);
        int16_t v   = static_cast<int16_t>((b1 << 8) | b0);
        return static_cast<double>(v);
    }

    case PollDataType::U32:
    {
        if (raw.size() < 4)
            return 0.0;
        uint16_t b0 = u8(raw[0]), b1 = u8(raw[1]);
        uint16_t b2 = u8(raw[2]), b3 = u8(raw[3]);
        uint32_t low  = static_cast<uint32_t>((b1 << 8) | b0);
        uint32_t high = static_cast<uint32_t>((b3 << 8) | b2);
        uint32_t v    = wordSwap ? ((low << 16) | high) : ((high << 16) | low);
        return static_cast<double>(v);
    }

    case PollDataType::S32:
    {
        if (raw.size() < 4)
            return 0.0;
        uint16_t b0 = u8(raw[0]), b1 = u8(raw[1]);
        uint16_t b2 = u8(raw[2]), b3 = u8(raw[3]);
        uint32_t low  = static_cast<uint32_t>((b1 << 8) | b0);
        uint32_t high = static_cast<uint32_t>((b3 << 8) | b2);
        int32_t v     = static_cast<int32_t>(wordSwap ? ((low << 16) | high) : ((high << 16) | low));
        return static_cast<double>(v);
    }

    case PollDataType::F32:
    {
        if (raw.size() < 4)
            return 0.0;
        uint16_t b0 = u8(raw[0]), b1 = u8(raw[1]);
        uint16_t b2 = u8(raw[2]), b3 = u8(raw[3]);
        uint32_t low  = static_cast<uint32_t>((b1 << 8) | b0);
        uint32_t high = static_cast<uint32_t>((b3 << 8) | b2);
        uint32_t u32  = wordSwap ? ((low << 16) | high) : ((high << 16) | low);
        float f       = 0.0f;
        std::memcpy(&f, &u32, sizeof(f));
        return static_cast<double>(f);
    }
    }

    return 0.0;
}

// ========================================================================
// Impl state & notification methods
// ========================================================================

void PLCController::Impl::setState(ConnState s)
{
    ConnState old = state_.exchange(s);
    if (old == s)
    {
        return;
    }

    LOGM_INFO(LOG_TAG, "State: {} -> {}", MagicEnum::enum_name(old), MagicEnum::enum_name(s));

    IPLCControllerCallbacks *cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        cb = callbacks_;
    }
    if (cb)
    {
        cb->onStateChanged(s);
    }
}

void PLCController::Impl::notifyConnected()
{
    IPLCControllerCallbacks *cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        cb = callbacks_;
    }
    if (cb)
    {
        cb->onConnected();
    }
}

void PLCController::Impl::notifyConnectionFailed(const std::string &err)
{
    IPLCControllerCallbacks *cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        cb = callbacks_;
    }
    if (cb)
    {
        cb->onConnectionFailed(err.c_str());
    }
}

void PLCController::Impl::notifyConnectionLost()
{
    IPLCControllerCallbacks *cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        cb = callbacks_;
    }
    if (cb)
    {
        cb->onConnectionLost();
    }
}

// ========================================================================
// Impl connection methods
// ========================================================================

bool PLCController::Impl::createAndConnectContext()
{
    std::lock_guard<std::mutex> lk(ctxMutex_);

    ctx_.reset();
    ctx_.reset(modbus_new_tcp(ip_.c_str(), static_cast<int>(port_)));
    if (!ctx_)
    {
        LOGM_ERROR(LOG_TAG, "modbus_new_tcp failed for {}:{}", ip_, port_);
        return false;
    }

    modbus_set_slave(ctx_.get(), unitId_);

    if (modbus_set_response_timeout(ctx_.get(), 0, 500000) == -1)
    {
        LOGM_ERROR(LOG_TAG, "modbus_set_response_timeout failed: {}", modbus_strerror(errno));
        ctx_.reset();
        return false;
    }

#if defined(LIBMODBUS_VERSION_MAJOR)
    modbus_set_byte_timeout(ctx_.get(), 0, 500000);
#endif

    if (modbus_connect(ctx_.get()) == -1)
    {
        LOGM_ERROR(LOG_TAG, "modbus_connect({}:{}) failed: {}", ip_, port_, modbus_strerror(errno));
        ctx_.reset();
        return false;
    }

    LOGM_INFO(LOG_TAG, "Connected to {}:{} (unitId={})", ip_, port_, unitId_);

    consecutiveErrors_ = 0;
    reconnectDelayMs_  = 500;
    hbFailCount_       = 0;
    hasLastHbValue_    = false;
    lastHbTime_        = SteadyClock::now();

    return true;
}

bool PLCController::Impl::reconnect()
{
    setState(ConnState::Reconnecting);
    LOGM_INFO(LOG_TAG, "Attempting to reconnect... (delay={}ms)", reconnectDelayMs_);
    {
        std::lock_guard<std::mutex> lk(ctxMutex_);
        ctx_.reset();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelayMs_));

    if (!workerRunning_)
    {
        LOGM_WARN(LOG_TAG, "Reconnect aborted – worker no longer running");
        return false;
    }

    if (!createAndConnectContext())
    {
        reconnectDelayMs_ = std::min(reconnectDelayMs_ * 2, maxReconnectDelayMs_);
        LOGM_WARN(LOG_TAG, "Reconnect failed, next delay={}ms", reconnectDelayMs_);
        return false;
    }

    LOGM_INFO(LOG_TAG, "Reconnect succeeded");
    setState(ConnState::Connected);
    notifyConnected();
    return true;
}

// ========================================================================
// Impl worker / queue methods
// ========================================================================

void PLCController::Impl::startWorkerIfNeeded()
{
    if (workerRunning_)
    {
        return;
    }

    workerRunning_ = true;
    worker_        = std::thread(&PLCController::Impl::ioLoop, this);
}

void PLCController::Impl::stopWorker()
{
    workerRunning_ = false;

    if (worker_.joinable())
    {
        worker_.join();
    }
}

void PLCController::Impl::startPollScheduler()
{
    pollSchedulerRunning_ = true;
}

void PLCController::Impl::stopPollScheduler()
{
    pollSchedulerRunning_ = false;
}

void PLCController::Impl::enqueueRead(const ReadJob &job)
{
    const bool pushed = readQueue_.push(job);
    if (!pushed)
    {
        LOGM_ERROR(LOG_TAG, "enqueueRead failed because queue is closed");
    }
}

void PLCController::Impl::enqueueWrite(const WriteJob &job)
{
    const bool pushed = writeQueue_.push(job);
    if (!pushed)
    {
        LOGM_ERROR(LOG_TAG, "enqueueWrite failed because queue is closed");
    }
}

void PLCController::Impl::clearWriteQueue()
{
    writeQueue_.clear();
}

// ========================================================================
// Impl read / write methods
// ========================================================================

bool PLCController::Impl::doRead(const ReadJob &job, ModbusResult &out, std::string &err)
{
    std::lock_guard<std::mutex> lk(ctxMutex_);

    if (!ctx_)
    {
        err = "modbus ctx is null";
        return false;
    }

    modbus_set_slave(ctx_.get(), job.unitId);

    out.type         = job.type;
    out.startAddress = job.address;
    out.count        = job.count;
    out.values.assign(job.count, 0);

    int rc = -1;

    switch (job.type)
    {
    case RegisterType::HoldingRegisters:
        rc = modbus_read_registers(ctx_.get(), job.address, job.count, out.values.data());
        break;

    case RegisterType::InputRegisters:
        rc = modbus_read_input_registers(ctx_.get(), job.address, job.count, out.values.data());
        break;

    case RegisterType::Coils:
    {
        std::vector<uint8_t> bits(job.count, 0);
        rc = modbus_read_bits(ctx_.get(), job.address, job.count, bits.data());
        if (rc != -1)
        {
            for (int i = 0; i < job.count; ++i)
            {
                out.values[i] = bits[i] ? 1 : 0;
            }
        }
        break;
    }

    case RegisterType::DiscreteInputs:
    {
        std::vector<uint8_t> bits(job.count, 0);
        rc = modbus_read_input_bits(ctx_.get(), job.address, job.count, bits.data());
        if (rc != -1)
        {
            for (int i = 0; i < job.count; ++i)
            {
                out.values[i] = bits[i] ? 1 : 0;
            }
        }
        break;
    }
    }

    if (rc == -1)
    {
        err = modbus_strerror(errno);
        LOGM_ERROR(LOG_TAG, "doRead failed addr={} count={} type={} err={}", job.address, job.count, static_cast<int>(job.type),
                   err);
        return false;
    }

    return true;
}

bool PLCController::Impl::doWrite(const WriteJob &job, std::string &err)
{
    std::lock_guard<std::mutex> lk(ctxMutex_);

    if (!ctx_)
    {
        err = "modbus ctx is null";
        return false;
    }

    modbus_set_slave(ctx_.get(), job.unitId);

    int rc = -1;

    switch (job.type)
    {
    case RegisterType::HoldingRegisters:
        if (job.values.empty())
        {
            err = "empty write values";
            return false;
        }
        if (job.values.size() == 1)
        {
            rc = modbus_write_register(ctx_.get(), job.address, job.values[0]);
        }
        else
        {
            rc = modbus_write_registers(ctx_.get(), job.address, static_cast<int>(job.values.size()),
                                        const_cast<uint16_t *>(job.values.data()));
        }
        break;

    case RegisterType::Coils:
        if (job.values.empty())
        {
            err = "empty write values";
            return false;
        }
        if (job.values.size() == 1)
        {
            rc = modbus_write_bit(ctx_.get(), job.address, job.values[0] ? 1 : 0);
        }
        else
        {
            std::vector<uint8_t> bits(job.values.size(), 0);
            for (size_t i = 0; i < job.values.size(); ++i)
            {
                bits[i] = job.values[i] ? 1 : 0;
            }
            rc = modbus_write_bits(ctx_.get(), job.address, static_cast<int>(bits.size()), bits.data());
        }
        break;

    case RegisterType::InputRegisters:
    case RegisterType::DiscreteInputs:
        err = "cannot write readonly register type";
        LOGM_ERROR(LOG_TAG, "doWrite attempted on readonly type={} addr={}", static_cast<int>(job.type), job.address);
        return false;
    }

    if (rc == -1)
    {
        err = modbus_strerror(errno);
        LOGM_ERROR(LOG_TAG, "doWrite failed addr={} count={} type={} err={}", job.address, job.values.size(),
                   static_cast<int>(job.type), err);
        return false;
    }

    return true;
}

// ========================================================================
// Impl heartbeat
// ========================================================================

bool PLCController::Impl::handleHeartbeatIfDue()
{
    if (!heartbeat_.enabled)
    {
        return true;
    }

    const auto now  = SteadyClock::now();
    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHbTime_).count();
    if (diff < heartbeat_.intervalMs)
    {
        return true;
    }

    lastHbTime_ = now;

    std::lock_guard<std::mutex> lk(ctxMutex_);
    if (!ctx_)
    {
        hbFailCount_++;
        LOGM_WARN(LOG_TAG, "Heartbeat #{} skipped – ctx is null", hbFailCount_);
        return hbFailCount_ < heartbeat_.failThreshold;
    }

    modbus_set_slave(ctx_.get(), unitId_);

    uint16_t val = 0;
    int rc       = modbus_read_registers(ctx_.get(), heartbeat_.address, 1, &val);
    if (rc == -1)
    {
        hbFailCount_++;
        LOGM_WARN(LOG_TAG, "Heartbeat #{} failed at addr={}: {}", hbFailCount_, heartbeat_.address, modbus_strerror(errno));
        return hbFailCount_ < heartbeat_.failThreshold;
    }

    if (heartbeat_.requireValueChange)
    {
        if (hasLastHbValue_ && val == lastHbValue_)
        {
            hbFailCount_++;
            LOGM_WARN(LOG_TAG, "Heartbeat #{} value unchanged at addr={} val={}", hbFailCount_, heartbeat_.address, val);
            lastHbValue_ = val;
            return hbFailCount_ < heartbeat_.failThreshold;
        }
        lastHbValue_    = val;
        hasLastHbValue_ = true;
    }

    hbFailCount_ = 0;
    return true;
}

// ========================================================================
// Impl poll methods
// ========================================================================

bool PLCController::Impl::isPollValueChanged(const PollItem &item, const std::vector<uint8_t> &newRaw)
{
    if (item.lastRaw.empty())
    {
        return true;
    }

    if (item.dataType == PollDataType::Bytes)
    {
        return item.lastRaw != newRaw;
    }

    double oldValue = extractNumericValue(item.lastRaw, item.dataType, item.wordSwap);
    double newValue = extractNumericValue(newRaw, item.dataType, item.wordSwap);

    if (std::isnan(oldValue) || std::isnan(newValue))
    {
        return true;
    }

    return std::fabs(newValue - oldValue) > item.deadband;
}

void PLCController::Impl::processPollCallback(PollId id, bool success, const ModbusResult &result, const std::string &error)
{
    PollItem snapshot;
    bool found = false;

    {
        std::lock_guard<std::mutex> lk(pollMutex_);
        auto it = pollItems_.find(id);
        if (it != pollItems_.end())
        {
            it->second.inFlight = false;
            it->second.nextDue  = SteadyClock::now() + std::chrono::milliseconds(it->second.intervalMs);
            snapshot            = it->second;
            found               = true;
        }
    }

    if (!found)
    {
        return;
    }

    if (!success)
    {
        LOGM_ERROR(LOG_TAG, "Poll {} failed: {}", id, error);

        IPLCControllerCallbacks *cb = nullptr;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_;
        }
        if (cb)
        {
            cb->onPollError(id, error.c_str());
        }
        return;
    }

    const std::vector<uint8_t> raw = resultToRaw(result);
    const TimestampMs ts           = TimeHelper::NowTimestamp();
    const float value              = static_cast<float>(extractNumericValue(raw, snapshot.dataType, snapshot.wordSwap));

    {
        IPLCControllerCallbacks *cb = nullptr;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_;
        }
        if (cb)
        {
            cb->onPollValue(id, raw.data(), static_cast<int>(raw.size()), ts);
        }
    }

    bool changed = true;
    if (snapshot.onChangeOnly)
    {
        changed = isPollValueChanged(snapshot, raw);
    }

    if (changed)
    {
        {
            std::lock_guard<std::mutex> lk(pollMutex_);
            auto it = pollItems_.find(id);
            if (it != pollItems_.end())
            {
                it->second.lastRaw = raw;
            }
        }

        IPLCControllerCallbacks *cb = nullptr;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_;
        }
        if (cb)
        {
            cb->onPollChanged(id, value, ts);
        }
    }
}

void PLCController::Impl::processPollSchedule()
{
    if (!pollSchedulerRunning_.load() || state_.load() != ConnState::Connected)
    {
        return;
    }

    const auto now = SteadyClock::now();
    std::vector<PollItem *> candidates;

    {
        std::lock_guard<std::mutex> lk(pollMutex_);
        for (auto &kv : pollItems_)
        {
            auto &item = kv.second;
            if (item.nextDue <= now)
            {
                if (skipIfInFlight_ && item.inFlight)
                {
                    continue;
                }
                if (!item.inFlight)
                {
                    candidates.push_back(&item);
                }
            }
        }
    }

    const int triggerCount = std::min(static_cast<int>(candidates.size()), maxPollsPerTick_);


    LOGM_DEBUG_IF(LOG_TAG, (pollLogging_ && triggerCount > 0), "Poll schedule: {} due, {} triggered this tick", candidates.size(),
                  triggerCount);

    for (int i = 0; i < triggerCount; ++i)
    {
        PollItem *item = candidates[i];
        item->inFlight = true;

        ReadJob job;
        job.type    = item->type;
        job.address = mapAddr_(item->address, multiplyAddrBy2_);
        job.count   = item->count;
        job.unitId  = item->unitId;
        job.tag     = item->tag;
        job.attempt = 0;

        const PollId pollId = item->id;
        job.onDone          = [this, pollId](bool success, const ModbusResult &result, const std::string &error)
        {
            processPollCallback(pollId, success, result, error);
        };

        enqueueRead(job);
    }
}

// ========================================================================
// Impl ioLoop
// ========================================================================

void PLCController::Impl::ioLoop()
{
    auto lastPollTick = SteadyClock::now();

    while (workerRunning_)
    {
        if (state_.load() == ConnState::Error || state_.load() == ConnState::Disconnected)
        {
            LOGM_INFO(LOG_TAG, "Connection error or disconnected,state = {}", MagicEnum::enum_name(state_.load()));

            while (workerRunning_)
            {
                if (reconnect())
                {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelayMs_));
            }

            continue;
        }

        if (state_.load() != ConnState::Connected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (!handleHeartbeatIfDue())
        {
            LOGM_WARN(LOG_TAG, "Heartbeat failure threshold reached, transitioning to Error");
            setState(ConnState::Error);
            clearWriteQueue();
            notifyConnectionLost();
            continue;
        }

        const auto now = SteadyClock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPollTick).count() >= pollTickMs_)
        {
            processPollSchedule();
            lastPollTick = now;
        }

        ReadJob readJob;
        WriteJob writeJob;
        bool hasRead  = false;
        bool hasWrite = false;

        if (writeQueue_.try_pop(writeJob))
        {
            hasWrite = true;
        }
        else if (readQueue_.try_pop(readJob))
        {
            hasRead = true;
        }
        else if (writeQueue_.pop_for(writeJob, std::chrono::milliseconds(5)))
        {
            hasWrite = true;
        }
        else if (readQueue_.pop_for(readJob, std::chrono::milliseconds(5)))
        {
            hasRead = true;
        }

        if (hasWrite)
        {
            std::string err;
            const bool ok = doWrite(writeJob, err);

            if (!ok)
            {
                consecutiveErrors_++;
                const bool severe = (errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE) || (errno == ENOTCONN) ||
                    (errno == ECONNREFUSED);

                if (severe || consecutiveErrors_ >= maxConsecutiveErrors_)
                {
                    LOGM_ERROR(LOG_TAG, "Write failed (severe={}, consecutive={}/{}), transitioning to Error: {}", severe,
                               consecutiveErrors_, maxConsecutiveErrors_, err);
                    setState(ConnState::Error);
                    clearWriteQueue();
                    notifyConnectionLost();
                }

                if (writeJob.attempt < retries_)
                {
                    LOGM_WARN(LOG_TAG, "Write retry {}/{} at addr={}", writeJob.attempt + 1, retries_, writeJob.address);
                    auto retry = writeJob;
                    retry.attempt++;
                    enqueueWrite(retry);
                }
                else if (writeJob.onDone)
                {
                    writeJob.onDone(false, err);
                }
            }
            else
            {
                consecutiveErrors_ = 0;
                if (writeJob.onDone)
                {
                    writeJob.onDone(true, "");
                }
            }

            if (writeGapMs_ > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(writeGapMs_));
            }
            continue;
        }

        if (hasRead)
        {
            ModbusResult out;
            std::string err;
            const bool ok = doRead(readJob, out, err);

            if (!ok)
            {
                consecutiveErrors_++;
                const bool severe = (errno == ETIMEDOUT) || (errno == ECONNRESET) || (errno == EPIPE) || (errno == ENOTCONN) ||
                    (errno == ECONNREFUSED);

                if (severe || consecutiveErrors_ >= maxConsecutiveErrors_)
                {
                    LOGM_ERROR(LOG_TAG, "Read failed (severe={}, consecutive={}/{}), transitioning to Error: {}", severe,
                               consecutiveErrors_, maxConsecutiveErrors_, err);
                    setState(ConnState::Error);
                    notifyConnectionLost();
                }

                if (readJob.attempt < retries_)
                {
                    LOGM_WARN(LOG_TAG, "Read retry {}/{} at addr={}", readJob.attempt + 1, retries_, readJob.address);
                    auto retry = readJob;
                    retry.attempt++;
                    enqueueRead(retry);
                }
                else if (readJob.onDone)
                {
                    readJob.onDone(false, ModbusResult{}, err);
                }
            }
            else
            {
                consecutiveErrors_ = 0;
                if (readJob.onDone)
                {
                    readJob.onDone(true, out, "");
                }
            }

            if (readGapMs_ > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(readGapMs_));
            }
            continue;
        }
    }
}

} // namespace SZXPLC
