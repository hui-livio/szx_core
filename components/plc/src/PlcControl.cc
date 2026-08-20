/**
 * @file PlcControl.cc
 * @brief PLC 控制工厂 — 创建跨 DLL 使用的 PLC 控制实例。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "PlcControlImpl.h"

#include <chrono>
#include <cstring>
#include "Logger.h"
#include "modbus.h"


namespace SZXPLC
{

// ========================================================================
// PLCController lifecycle
// ========================================================================

PLCController::PLCController() :
    impl_(std::make_unique<Impl>())
{
    impl_->lastHbTime_ = SteadyClock::now();
}

PLCController::~PLCController()
{
    disconnectPLC();
    impl_.reset(); // 释放资源
}

// ========================================================================
// PLCController connection
// ========================================================================

bool PLCController::connectToPLC(const char *ip, uint16_t port)
{
    LOGM_INFO(LOG_TAG, "connectToPLC({}:{})", ip, port);

    impl_->ip_   = ip;
    impl_->port_ = port;

    impl_->setState(ConnState::Connecting);

    {
        std::lock_guard<std::mutex> lk(impl_->ctxMutex_);
        impl_->ctx_.reset();
    }

    if (!impl_->createAndConnectContext())
    {
        impl_->setState(ConnState::Error);
        impl_->notifyConnectionFailed(modbus_strerror(errno));
        return false;
    }

    impl_->setState(ConnState::Connected);
    impl_->notifyConnected();

    impl_->startWorkerIfNeeded();
    impl_->startPollScheduler();
    return true;
}

void PLCController::disconnectPLC()
{
    impl_->stopPollScheduler();
    impl_->stopWorker();

    impl_->readQueue_.clear();
    impl_->writeQueue_.clear();

    {
        std::lock_guard<std::mutex> lk(impl_->pollMutex_);
        for (auto &kv : impl_->pollItems_)
        {
            kv.second.inFlight = false;
        }
    }

    {
        std::lock_guard<std::mutex> lk(impl_->ctxMutex_);
        impl_->ctx_.reset();
    }

    impl_->setState(ConnState::Disconnected);
    LOGM_INFO(LOG_TAG, "disconnectPLC");
}

// ========================================================================
// PLCController configuration
// ========================================================================

void PLCController::setUnitId(int uid)
{
    impl_->unitId_ = uid;
}

void PLCController::setReadGapMs(int ms)
{
    impl_->readGapMs_ = ms;
}

void PLCController::setWriteGapMs(int ms)
{
    impl_->writeGapMs_ = ms;
}

void PLCController::setRetries(int n)
{
    impl_->retries_ = n;
}

void PLCController::setAddressMultiplyBy2(bool v)
{
    impl_->multiplyAddrBy2_ = v;
}

void PLCController::setDefaultWordSwap(bool v)
{
    impl_->defaultWordSwap_ = v;
}

void PLCController::setPollTickMs(int ms)
{
    impl_->pollTickMs_ = ms;
}

void PLCController::setMaxPollsPerTick(int n)
{
    impl_->maxPollsPerTick_ = n;
}

void PLCController::setSkipIfInFlight(bool on)
{
    impl_->skipIfInFlight_ = on;
}

void PLCController::setPollLogging(bool on)
{
    impl_->pollLogging_ = on;
}

void PLCController::setCallbacks(IPLCControllerCallbacks *callbacks)
{
    LOGM_DEBUG(LOG_TAG, "setCallbacks ptr={}", static_cast<const void *>(callbacks));
    std::lock_guard<std::mutex> lk(impl_->cbMutex_);
    impl_->callbacks_ = callbacks;
}

// ========================================================================
// PLCController state
// ========================================================================

bool PLCController::isRunning() const
{
    return impl_->state_.load() == ConnState::Connected;
}

ConnState PLCController::state() const
{
    return impl_->state_.load();
}

// ========================================================================
// PLCController write operations
// ========================================================================

void PLCController::writeInt(uint16_t address, int16_t data)
{
    LOGM_DEBUG(LOG_TAG, "writeInt addr={} data={}", address, data);
    WriteJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.values  = {static_cast<uint16_t>(data)};
    job.unitId  = impl_->unitId_;
    job.tag     = "writeInt";
    impl_->enqueueWrite(job);
}

void PLCController::writeFloat(uint16_t address, float data, bool wordSwap)
{
    LOGM_DEBUG(LOG_TAG, "writeFloat addr={} data={} wordSwap={}", address, data, wordSwap);
    const bool doSwap = wordSwap ? true : impl_->defaultWordSwap_;
    uint16_t low, high;
    packFloatToRegs(data, doSwap, low, high);

    WriteJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.values  = {low, high};
    job.unitId  = impl_->unitId_;
    job.tag     = "writeFloat";
    impl_->enqueueWrite(job);
}

void PLCController::writeRegisters(uint16_t startAddress0, const uint16_t *regs, size_t count)
{
    if (!regs || count == 0)
    {
        LOGM_WARN(LOG_TAG, "writeRegisters skipped – null ptr or count=0");
        return;
    }

    LOGM_DEBUG(LOG_TAG, "writeRegisters addr={} count={}", startAddress0, count);
    WriteJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(startAddress0, impl_->multiplyAddrBy2_);
    job.values.assign(regs, regs + count);
    job.unitId = impl_->unitId_;
    job.tag    = "writeRegisters";
    impl_->enqueueWrite(job);
}

void PLCController::writeFloatArray(uint16_t startAddress0, const float *values, size_t count, bool wordSwap)
{
    if (!values || count == 0)
    {
        LOGM_WARN(LOG_TAG, "writeFloatArray skipped – null ptr or count=0");
        return;
    }

    LOGM_DEBUG(LOG_TAG, "writeFloatArray addr={} count={} wordSwap={}", startAddress0, count, wordSwap);
    const bool doSwap = wordSwap ? true : impl_->defaultWordSwap_;
    std::vector<uint16_t> regs;
    regs.reserve(count * 2);

    for (size_t i = 0; i < count; ++i)
    {
        uint16_t lo, hi;
        packFloatToRegs(values[i], doSwap, lo, hi);
        regs.push_back(lo);
        regs.push_back(hi);
    }

    WriteJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(startAddress0, impl_->multiplyAddrBy2_);
    job.values  = std::move(regs);
    job.unitId  = impl_->unitId_;
    job.tag     = "writeFloatArray";
    impl_->enqueueWrite(job);
}

// ========================================================================
// PLCController read operations
// ========================================================================

void PLCController::readRegisters(uint16_t address, uint16_t registerCount)
{
    LOGM_DEBUG(LOG_TAG, "readRegisters addr={} count={}", address, registerCount);
    ReadJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.count   = registerCount;
    job.unitId  = impl_->unitId_;
    job.tag     = "readRegisters";
    impl_->enqueueRead(job);
}

bool PLCController::readRegistersSync(uint16_t address, uint16_t count, uint16_t *outValues)
{
    if (!outValues || count == 0)
        return false;

    ReadJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.count   = count;
    job.unitId  = impl_->unitId_;
    job.tag     = "readRegistersSync";

    ModbusResult result;
    std::string err;
    if (!impl_->doRead(job, result, err))
        return false;

    size_t n = std::min(static_cast<size_t>(count), result.values.size());
    std::copy_n(result.values.begin(), n, outValues);
    return true;
}

bool PLCController::writeRegisterSync(uint16_t address, uint16_t value)
{
    WriteJob job;
    job.type    = RegisterType::HoldingRegisters;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.values  = {value};
    job.unitId  = impl_->unitId_;
    job.tag     = "writeRegisterSync";

    std::string err;
    return impl_->doWrite(job, err);
}

bool PLCController::readCoilsSync(uint16_t address, uint16_t count, uint8_t *outBits)
{
    if (!outBits || count == 0)
        return false;

    ReadJob job;
    job.type    = RegisterType::Coils;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.count   = count;
    job.unitId  = impl_->unitId_;
    job.tag     = "readCoilsSync";

    ModbusResult result;
    std::string err;
    if (!impl_->doRead(job, result, err))
        return false;

    for (size_t i = 0; i < count && i < result.values.size(); ++i)
        outBits[i] = result.values[i] ? 1 : 0;
    return true;
}

bool PLCController::writeCoilSync(uint16_t address, bool value)
{
    WriteJob job;
    job.type    = RegisterType::Coils;
    job.address = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    job.values  = {static_cast<uint16_t>(value ? 1 : 0)};
    job.unitId  = impl_->unitId_;
    job.tag     = "writeCoilSync";

    std::string err;
    return impl_->doWrite(job, err);
}

// ========================================================================
// PLCController heartbeat
// ========================================================================

void PLCController::enableHeartbeat(uint16_t address, int intervalMs, int failThreshold, bool requireValueChange)
{
    LOGM_INFO(LOG_TAG, "enableHeartbeat addr={} interval={} threshold={} requireChange={}", address, intervalMs, failThreshold,
              requireValueChange);

    impl_->heartbeat_.enabled            = true;
    impl_->heartbeat_.address            = Impl::mapAddr_(address, impl_->multiplyAddrBy2_);
    impl_->heartbeat_.intervalMs         = std::max(100, intervalMs);
    impl_->heartbeat_.failThreshold      = std::max(1, failThreshold);
    impl_->heartbeat_.requireValueChange = requireValueChange;

    impl_->hbFailCount_    = 0;
    impl_->hasLastHbValue_ = false;
    impl_->lastHbTime_     = std::chrono::steady_clock::now();
}

void PLCController::disableHeartbeat()
{
    LOGM_INFO(LOG_TAG, "disableHeartbeat");
    impl_->heartbeat_.enabled = false;
    impl_->hbFailCount_       = 0;
    impl_->hasLastHbValue_    = false;
}

// ========================================================================
// PLCController poll scheduler
// ========================================================================

void PLCController::startPollScheduler()
{
    impl_->startPollScheduler();
}

void PLCController::stopPollScheduler()
{
    impl_->stopPollScheduler();
}

PollId PLCController::addPoll(uint16_t address, uint16_t count, int intervalMs, const PollOptions *opt)
{
    if (intervalMs <= 0 || count < 1)
    {
        return 0;
    }

    PollOptions defaultOpt;
    if (!opt)
    {
        opt = &defaultOpt;
    }

    const uint16_t normalizedCount = std::max(count, requiredRegisterCountForDataType(opt->dataType));

    PollItem item;
    item.id           = impl_->nextPollId_.fetch_add(1);
    item.unitId       = opt->unitId;
    item.type         = opt->type;
    item.address      = address;
    item.count        = normalizedCount;
    item.intervalMs   = intervalMs;
    item.onChangeOnly = opt->onChangeOnly;
    item.dataType     = opt->dataType;
    item.wordSwap     = opt->wordSwap;
    item.deadband     = opt->deadband;
    item.tag          = opt->tag ? opt->tag : "poll";
    item.nextDue      = SteadyClock::now() + std::chrono::milliseconds(intervalMs);

    std::lock_guard<std::mutex> lk(impl_->pollMutex_);
    impl_->pollItems_[item.id] = std::move(item);
    LOGM_INFO(LOG_TAG, "addPoll id={} addr={} count={} interval={}ms tag={}", item.id, address, normalizedCount, intervalMs,
              item.tag);
    return item.id;
}

bool PLCController::updatePoll(PollId id, const int *intervalMs, const PollOptions *opt)
{
    std::lock_guard<std::mutex> lk(impl_->pollMutex_);
    auto it = impl_->pollItems_.find(id);
    if (it == impl_->pollItems_.end())
    {
        LOGM_WARN(LOG_TAG, "updatePoll id={} not found", id);
        return false;
    }

    auto &item = it->second;

    if (intervalMs)
    {
        if (*intervalMs <= 0)
        {
            return false;
        }
        item.intervalMs = *intervalMs;
        item.nextDue    = SteadyClock::now() + std::chrono::milliseconds(item.intervalMs);
    }

    if (opt)
    {
        item.unitId       = opt->unitId;
        item.type         = opt->type;
        item.onChangeOnly = opt->onChangeOnly;
        item.dataType     = opt->dataType;
        item.wordSwap     = opt->wordSwap;
        item.deadband     = opt->deadband;
        item.tag          = opt->tag ? opt->tag : item.tag;
    }

    return true;
}

bool PLCController::removePoll(PollId id)
{
    std::lock_guard<std::mutex> lk(impl_->pollMutex_);
    const bool removed = impl_->pollItems_.erase(id) > 0;
    LOGM_DEBUG(LOG_TAG, "removePoll id={} {}", id, removed ? "ok" : "not found");
    return removed;
}

void PLCController::clearPolls()
{
    std::lock_guard<std::mutex> lk(impl_->pollMutex_);
    const size_t n = impl_->pollItems_.size();
    impl_->pollItems_.clear();
    LOGM_DEBUG(LOG_TAG, "clearPolls removed {} items", n);
}

// ========================================================================
// PLCController static utilities
// ========================================================================

void PLCController::packFloatToRegs(float v, bool wordSwap, uint16_t &outLow, uint16_t &outHigh)
{
    uint32_t u = 0;
    std::memcpy(&u, &v, sizeof(u));

    uint16_t low  = static_cast<uint16_t>(u & 0xFFFF);
    uint16_t high = static_cast<uint16_t>((u >> 16) & 0xFFFF);

    if (wordSwap)
    {
        std::swap(low, high);
    }
    outLow  = low;
    outHigh = high;
}

void PLCController::floatToBytes(float data, uint8_t outBytes[4])
{
    std::memcpy(outBytes, &data, 4);
}

} // namespace SZXPLC
