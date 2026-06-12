#include "plc/plccontrol.h"
#include "common/log/log_macros.h"

namespace SZXPLC
{
    namespace {
        uint16_t requiredRegisterCountForDataType(PollDataType dataType)
        {
            switch (dataType) {
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
    }

    PLCController::PLCController() {
        lastHbTime_ = SteadyClock::now();
    }

    PLCController::~PLCController() {
        disconnectPLC();
    }

    void PLCController::setCallbacks(Callbacks callbacks) {
        std::lock_guard<std::mutex> lk(cbMutex_);
        callbacks_ = std::move(callbacks);
    }

    uint16_t PLCController::mapAddr_(uint16_t addr, bool mul2) {
        return mul2 ? static_cast<uint16_t>(addr * 2) : addr;
    }

    TimestampMs PLCController::nowSystemMs()  {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                SystemClock::now().time_since_epoch())
            .count();
    }

    std::pair<uint16_t, uint16_t> PLCController::packFloatToRegs(float v, bool wordSwap) {
        uint32_t u = 0;
        std::memcpy(&u, &v, sizeof(u));

        uint16_t low = static_cast<uint16_t>(u & 0xFFFF);
        uint16_t high = static_cast<uint16_t>((u >> 16) & 0xFFFF);

        if (wordSwap) {
            std::swap(low, high);
        }
        return {low, high};
    }

    std::vector<uint8_t> PLCController::floatToBytes(float data) {
        std::vector<uint8_t> bytes(4);
        std::memcpy(bytes.data(), &data, 4);
        return bytes;
    }

    void PLCController::setState(ConnState s) {
        ConnState old = state_.exchange(s);
        if (old == s) {
            return;
        }

        std::function<void(ConnState)> cb;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_.onStateChanged;
        }
        if (cb) {
            cb(s);
        }
    }

    void PLCController::notifyConnected() {
        std::function<void()> cb;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_.onConnected;
        }
        if (cb) {
            cb();
        }
    }

    void PLCController::notifyConnectionFailed(const std::string& err) {
        std::function<void(const std::string&)> cb;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_.onConnectionFailed;
        }
        if (cb) {
            cb(err);
        }
    }

    void PLCController::notifyConnectionLost() {
        std::function<void()> cb;
        {
            std::lock_guard<std::mutex> lk(cbMutex_);
            cb = callbacks_.onConnectionLost;
        }
        if (cb) {
            cb();
        }
    }

    bool PLCController::createAndConnectContext() {
        std::lock_guard<std::mutex> lk(ctxMutex_);

        ctx_.reset(modbus_new_tcp(ip_.c_str(), static_cast<int>(port_)));
        if (!ctx_) {
            return false;
        }

        modbus_set_slave(ctx_.get(), unitId_);

        if (modbus_set_response_timeout(ctx_.get(), 0, 500000) == -1) {
            ctx_.reset();
            return false;
        }

    #if defined(LIBMODBUS_VERSION_MAJOR)
        modbus_set_byte_timeout(ctx_.get(), 0, 500000);
    #endif

        if (modbus_connect(ctx_.get()) == -1) {
            ctx_.reset();
            return false;
        }

        consecutiveErrors_ = 0;
        reconnectDelayMs_ = 500;
        hbFailCount_ = 0;
        hasLastHbValue_ = false;
        lastHbTime_ = SteadyClock::now();

        return true;
    }

    bool PLCController::connectToPLC(const std::string& ip, uint16_t port) {
        ip_ = ip;
        port_ = port;

        setState(ConnState::Connecting);

        {
            std::lock_guard<std::mutex> lk(ctxMutex_);
            ctx_.reset();
        }

        if (!createAndConnectContext()) {
            setState(ConnState::Error);
            notifyConnectionFailed(modbus_strerror(errno));
            return false;
        }

        setState(ConnState::Connected);
        notifyConnected();

        startWorkerIfNeeded();
        startPollScheduler();
        return true;
    }

    void PLCController::disconnectPLC() {
        stopPollScheduler();
        stopWorker();

        readQueue_.clear();
        writeQueue_.clear();

        {
            std::lock_guard<std::mutex> lk(pollMutex_);
            for (auto& kv : pollItems_) {
                kv.second.inFlight = false;
            }
        }

        {
            std::lock_guard<std::mutex> lk(ctxMutex_);
            ctx_.reset();
        }

        setState(ConnState::Disconnected);
    }

    bool PLCController::reconnect() {
        setState(ConnState::Reconnecting);
        LOG_INFO("[PLC] Attempting to reconnect...");
        {
            std::lock_guard<std::mutex> lk(ctxMutex_);
            ctx_.reset();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelayMs_));

        if (!workerRunning_) {
            return false;
        }

        if (!createAndConnectContext()) {
            reconnectDelayMs_ = std::min(reconnectDelayMs_ * 2, maxReconnectDelayMs_);
            return false;
        }

        setState(ConnState::Connected);
        notifyConnected();
        return true;
    }

    void PLCController::startWorkerIfNeeded() {
        if (workerRunning_) {
            return;
        }

        workerRunning_ = true;
        worker_ = std::thread(&PLCController::ioLoop, this);
    }

    void PLCController::stopWorker() {
        workerRunning_ = false;

        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void PLCController::enqueueRead(const ReadJob& job) {
        const bool pushed = readQueue_.push(job);
        if (!pushed) {
            LOG_ERROR("[PLC] enqueueRead failed because queue is closed");
        }
    }

    void PLCController::enqueueWrite(const WriteJob& job) {
        const bool pushed = writeQueue_.push(job);
        if (!pushed) {
            LOG_ERROR("[PLC] enqueueWrite failed because queue is closed");
        }
    }

    void PLCController::writeInt(uint16_t address, int16_t data) {
        WriteJob job;
        job.type = RegisterType::HoldingRegisters;
        job.address = mapAddr_(address, multiplyAddrBy2_);
        job.values = {static_cast<uint16_t>(data)};
        job.unitId = unitId_;
        job.tag = "writeInt";
        enqueueWrite(job);
    }

    void PLCController::writeFloat(uint16_t address, float data, bool wordSwap) {
        const bool doSwap = wordSwap ? true : defaultWordSwap_;
        auto [low, high] = packFloatToRegs(data, doSwap);

        WriteJob job;
        job.type = RegisterType::HoldingRegisters;
        job.address = mapAddr_(address, multiplyAddrBy2_);
        job.values = {low, high};
        job.unitId = unitId_;
        job.tag = "writeFloat";
        enqueueWrite(job);
    }

    void PLCController::writeRegisters(uint16_t startAddress0, const std::vector<uint16_t>& regs) {
        if (regs.empty()) {
            return;
        }

        WriteJob job;
        job.type = RegisterType::HoldingRegisters;
        job.address = mapAddr_(startAddress0, multiplyAddrBy2_);
        job.values = regs;
        job.unitId = unitId_;
        job.tag = "writeRegisters";
        enqueueWrite(job);
    }

    void PLCController::writeFloatArray(uint16_t startAddress0, const std::vector<float>& values, bool wordSwap) {
        if (values.empty()) {
            return;
        }

        const bool doSwap = wordSwap ? true : defaultWordSwap_;
        std::vector<uint16_t> regs;
        regs.reserve(values.size() * 2);

        for (float f : values) {
            auto [lo, hi] = packFloatToRegs(f, doSwap);
            regs.push_back(lo);
            regs.push_back(hi);
        }

        WriteJob job;
        job.type = RegisterType::HoldingRegisters;
        job.address = mapAddr_(startAddress0, multiplyAddrBy2_);
        job.values = std::move(regs);
        job.unitId = unitId_;
        job.tag = "writeFloatArray";
        enqueueWrite(job);
    }

    void PLCController::readRegisters(uint16_t address, uint16_t registerCount) {
        ReadJob job;
        job.type = RegisterType::HoldingRegisters;
        job.address = mapAddr_(address, multiplyAddrBy2_);
        job.count = registerCount;
        job.unitId = unitId_;
        job.tag = "readRegisters";
        enqueueRead(job);
    }

    bool PLCController::doRead(const ReadJob& job, ModbusResult& out, std::string& err) {
        std::lock_guard<std::mutex> lk(ctxMutex_);

        if (!ctx_) {
            err = "modbus ctx is null";
            return false;
        }

        modbus_set_slave(ctx_.get(), job.unitId);

        out.type = job.type;
        out.startAddress = job.address;
        out.count = job.count;
        out.values.assign(job.count, 0);

        int rc = -1;

        switch (job.type) {
            case RegisterType::HoldingRegisters:
                rc = modbus_read_registers(ctx_.get(), job.address, job.count, out.values.data());
                break;

            case RegisterType::InputRegisters:
                rc = modbus_read_input_registers(ctx_.get(), job.address, job.count, out.values.data());
                break;

            case RegisterType::Coils: {
                std::vector<uint8_t> bits(job.count, 0);
                rc = modbus_read_bits(ctx_.get(), job.address, job.count, bits.data());
                if (rc != -1) {
                    for (int i = 0; i < job.count; ++i) {
                        out.values[i] = bits[i] ? 1 : 0;
                    }
                }
                break;
            }

            case RegisterType::DiscreteInputs: {
                std::vector<uint8_t> bits(job.count, 0);
                rc = modbus_read_input_bits(ctx_.get(), job.address, job.count, bits.data());
                if (rc != -1) {
                    for (int i = 0; i < job.count; ++i) {
                        out.values[i] = bits[i] ? 1 : 0;
                    }
                }
                break;
            }
        }

        if (rc == -1) {
            err = modbus_strerror(errno);
            return false;
        }

        return true;
    }

    bool PLCController::doWrite(const WriteJob& job, std::string& err) {
        std::lock_guard<std::mutex> lk(ctxMutex_);

        if (!ctx_) {
            err = "modbus ctx is null";
            return false;
        }

        modbus_set_slave(ctx_.get(), job.unitId);

        int rc = -1;

        switch (job.type) {
            case RegisterType::HoldingRegisters:
                if (job.values.empty()) {
                    err = "empty write values";
                    return false;
                }
                if (job.values.size() == 1) {
                    rc = modbus_write_register(ctx_.get(), job.address, job.values[0]);
                } else {
                    rc = modbus_write_registers(
                        ctx_.get(),
                        job.address,
                        static_cast<int>(job.values.size()),
                        const_cast<uint16_t*>(job.values.data()));
                }
                break;

            case RegisterType::Coils:
                if (job.values.empty()) {
                    err = "empty write values";
                    return false;
                }
                if (job.values.size() == 1) {
                    rc = modbus_write_bit(ctx_.get(), job.address, job.values[0] ? 1 : 0);
                } else {
                    std::vector<uint8_t> bits(job.values.size(), 0);
                    for (size_t i = 0; i < job.values.size(); ++i) {
                        bits[i] = job.values[i] ? 1 : 0;
                    }
                    rc = modbus_write_bits(ctx_.get(), job.address, static_cast<int>(bits.size()), bits.data());
                }
                break;

            case RegisterType::InputRegisters:
            case RegisterType::DiscreteInputs:
                err = "cannot write readonly register type";
                return false;
        }

        if (rc == -1) {
            err = modbus_strerror(errno);
            return false;
        }

        return true;
    }

    void PLCController::enableHeartbeat(uint16_t address, int intervalMs, int failThreshold, bool requireValueChange) {
        heartbeat_.enabled = true;
        heartbeat_.address = mapAddr_(address, multiplyAddrBy2_);
        heartbeat_.intervalMs = std::max(100, intervalMs);
        heartbeat_.failThreshold = std::max(1, failThreshold);
        heartbeat_.requireValueChange = requireValueChange;

        hbFailCount_ = 0;
        hasLastHbValue_ = false;
        lastHbTime_ = SteadyClock::now();
    }

    void PLCController::disableHeartbeat() {
        heartbeat_.enabled = false;
        hbFailCount_ = 0;
        hasLastHbValue_ = false;
    }

    bool PLCController::handleHeartbeatIfDue() {
        if (!heartbeat_.enabled) {
            return true;
        }

        const auto now = SteadyClock::now();
        const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHbTime_).count();
        if (diff < heartbeat_.intervalMs) {
            return true;
        }

        lastHbTime_ = now;

        std::lock_guard<std::mutex> lk(ctxMutex_);
        if (!ctx_) {
            hbFailCount_++;
            return hbFailCount_ < heartbeat_.failThreshold;
        }

        modbus_set_slave(ctx_.get(), unitId_);

        uint16_t val = 0;
        int rc = modbus_read_registers(ctx_.get(), heartbeat_.address, 1, &val);
        if (rc == -1) {
            hbFailCount_++;
            return hbFailCount_ < heartbeat_.failThreshold;
        }

        if (heartbeat_.requireValueChange) {
            if (hasLastHbValue_ && val == lastHbValue_) {
                hbFailCount_++;
                lastHbValue_ = val;
                return hbFailCount_ < heartbeat_.failThreshold;
            }
            lastHbValue_ = val;
            hasLastHbValue_ = true;
        }

        hbFailCount_ = 0;
        return true;
    }

    void PLCController::clearWriteQueue() {
        writeQueue_.clear();
    }

    void PLCController::startPollScheduler() {
        pollSchedulerRunning_ = true;
    }

    void PLCController::stopPollScheduler() {
        pollSchedulerRunning_ = false;
    }

    PollId PLCController::addPoll(uint16_t address, uint16_t count, int intervalMs, const PollOptions& opt) {
        if (intervalMs <= 0 || count < 1) {
            return 0;
        }

        const uint16_t normalizedCount = std::max(count, requiredRegisterCountForDataType(opt.dataType));

        PollItem item;
        item.id = nextPollId_.fetch_add(1);
        item.unitId = opt.unitId;
        item.type = opt.type;
        item.address = address;
        item.count = normalizedCount;
        item.intervalMs = intervalMs;
        item.onChangeOnly = opt.onChangeOnly;
        item.dataType = opt.dataType;
        item.wordSwap = opt.wordSwap;
        item.deadband = opt.deadband;
        item.tag = opt.tag;
        item.nextDue = SteadyClock::now() + std::chrono::milliseconds(intervalMs);

        std::lock_guard<std::mutex> lk(pollMutex_);
        pollItems_[item.id] = std::move(item);
        return item.id;
    }

    bool PLCController::updatePoll(PollId id, std::optional<int> intervalMs, std::optional<PollOptions> opt) {
        std::lock_guard<std::mutex> lk(pollMutex_);
        auto it = pollItems_.find(id);
        if (it == pollItems_.end()) {
            return false;
        }

        auto& item = it->second;

        if (intervalMs.has_value()) {
            if (*intervalMs <= 0) {
                return false;
            }
            item.intervalMs = *intervalMs;
            item.nextDue = SteadyClock::now() + std::chrono::milliseconds(item.intervalMs);
        }

        if (opt.has_value()) {
            const auto& o = *opt;
            item.unitId = o.unitId;
            item.type = o.type;
            item.onChangeOnly = o.onChangeOnly;
            item.dataType = o.dataType;
            item.wordSwap = o.wordSwap;
            item.deadband = o.deadband;
            item.tag = o.tag;
        }

        return true;
    }

    bool PLCController::removePoll(PollId id) {
        std::lock_guard<std::mutex> lk(pollMutex_);
        return pollItems_.erase(id) > 0;
    }

    void PLCController::clearPolls() {
        std::lock_guard<std::mutex> lk(pollMutex_);
        pollItems_.clear();
    }

    std::vector<uint8_t> PLCController::resultToRaw(const ModbusResult& result) {
        std::vector<uint8_t> raw(result.count * 2);
        for (size_t i = 0; i < result.values.size(); ++i) {
            uint16_t v = result.values[i];
            raw[i * 2] = static_cast<uint8_t>(v & 0xFF);
            raw[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        }
        return raw;
    }

    double PLCController::extractNumericValue(const std::vector<uint8_t>& raw, PollDataType dataType, bool wordSwap) {
        auto u8 = [](uint8_t c) -> uint16_t { return static_cast<uint16_t>(c); };

        if (raw.size() < 2) {
            return 0.0;
        }

        switch (dataType) {
            case PollDataType::Bytes:
                return 0.0;

            case PollDataType::U16: {
                uint16_t b0 = u8(raw[0]);
                uint16_t b1 = u8(raw[1]);
                uint16_t v = static_cast<uint16_t>((b1 << 8) | b0);
                return static_cast<double>(v);
            }

            case PollDataType::S16: {
                uint16_t b0 = u8(raw[0]);
                uint16_t b1 = u8(raw[1]);
                int16_t v = static_cast<int16_t>((b1 << 8) | b0);
                return static_cast<double>(v);
            }

            case PollDataType::U32: {
                if (raw.size() < 4) return 0.0;
                uint16_t b0 = u8(raw[0]), b1 = u8(raw[1]);
                uint16_t b2 = u8(raw[2]), b3 = u8(raw[3]);
                uint32_t low = static_cast<uint32_t>((b1 << 8) | b0);
                uint32_t high = static_cast<uint32_t>((b3 << 8) | b2);
                uint32_t v = wordSwap ? ((low << 16) | high) : ((high << 16) | low);
                return static_cast<double>(v);
            }

            case PollDataType::S32: {
                if (raw.size() < 4) return 0.0;
                uint16_t b0 = u8(raw[0]), b1 = u8(raw[1]);
                uint16_t b2 = u8(raw[2]), b3 = u8(raw[3]);
                uint32_t low = static_cast<uint32_t>((b1 << 8) | b0);
                uint32_t high = static_cast<uint32_t>((b3 << 8) | b2);
                int32_t v = static_cast<int32_t>(wordSwap ? ((low << 16) | high) : ((high << 16) | low));
                return static_cast<double>(v);
            }

            case PollDataType::F32: {
                if (raw.size() < 4) return 0.0;
                uint16_t b0 = u8(raw[0]), b1 = u8(raw[1]);
                uint16_t b2 = u8(raw[2]), b3 = u8(raw[3]);
                uint32_t low = static_cast<uint32_t>((b1 << 8) | b0);
                uint32_t high = static_cast<uint32_t>((b3 << 8) | b2);
                uint32_t u32 = wordSwap ? ((low << 16) | high) : ((high << 16) | low);
                float f = 0.0f;
                std::memcpy(&f, &u32, sizeof(f));
                return static_cast<double>(f);
            }
        }

        return 0.0;
    }

    bool PLCController::isPollValueChanged(const PollItem& item, const std::vector<uint8_t>& newRaw) {
        if (item.lastRaw.empty()) {
            return true;
        }

        if (item.dataType == PollDataType::Bytes) {
            return item.lastRaw != newRaw;
        }

        double oldValue = extractNumericValue(item.lastRaw, item.dataType, item.wordSwap);
        double newValue = extractNumericValue(newRaw, item.dataType, item.wordSwap);

        if (std::isnan(oldValue) || std::isnan(newValue)) {
            return true;
        }

        return std::fabs(newValue - oldValue) > item.deadband;
    }

    void PLCController::processPollCallback(PollId id, bool success, const ModbusResult& result, const std::string& error) {
        PollItem snapshot;
        bool found = false;

        {
            std::lock_guard<std::mutex> lk(pollMutex_);
            auto it = pollItems_.find(id);
            if (it != pollItems_.end()) {
                it->second.inFlight = false;
                it->second.nextDue = SteadyClock::now() + std::chrono::milliseconds(it->second.intervalMs);
                snapshot = it->second;
                found = true;
            }
        }

        if (!found) {
            return;
        }

        if (!success) {
            std::function<void(PollId, const std::string&)> cb;
            {
                std::lock_guard<std::mutex> lk(cbMutex_);
                cb = callbacks_.onPollError;
            }
            if (cb) {
                cb(id, error);
            }
            return;
        }

        const std::vector<uint8_t> raw = resultToRaw(result);
        const TimestampMs ts = nowSystemMs();
        const float value = static_cast<float>(extractNumericValue(raw, snapshot.dataType, snapshot.wordSwap));

        {
            std::function<void(PollId, const std::vector<uint8_t>&, TimestampMs)> cb;
            {
                std::lock_guard<std::mutex> lk(cbMutex_);
                cb = callbacks_.onPollValue;
            }
            if (cb) {
                cb(id, raw, ts);
            }
        }

        bool changed = true;
        if (snapshot.onChangeOnly) {
            changed = isPollValueChanged(snapshot, raw);
        }

        if (changed) {
            {
                std::lock_guard<std::mutex> lk(pollMutex_);
                auto it = pollItems_.find(id);
                if (it != pollItems_.end()) {
                    it->second.lastRaw = raw;
                }
            }

            std::function<void(PollId, float, TimestampMs)> cb;
            {
                std::lock_guard<std::mutex> lk(cbMutex_);
                cb = callbacks_.onPollChanged;
            }
            if (cb) {
                cb(id, value, ts);
            }
        }
    }

    void PLCController::processPollSchedule() {
        if (!pollSchedulerRunning_.load() || state_.load() != ConnState::Connected) {
            return;
        }

        const auto now = SteadyClock::now();
        std::vector<PollItem*> candidates;

        {
            std::lock_guard<std::mutex> lk(pollMutex_);
            for (auto& kv : pollItems_) {
                auto& item = kv.second;
                if (item.nextDue <= now) {
                    if (skipIfInFlight_ && item.inFlight) {
                        continue;
                    }
                    if (!item.inFlight) {
                        candidates.push_back(&item);
                    }
                }
            }
        }

        const int triggerCount = std::min(static_cast<int>(candidates.size()), maxPollsPerTick_);

        for (int i = 0; i < triggerCount; ++i) {
            PollItem* item = candidates[i];
            item->inFlight = true;

            ReadJob job;
            job.type = item->type;
            job.address = mapAddr_(item->address, multiplyAddrBy2_);
            job.count = item->count;
            job.unitId = item->unitId;
            job.tag = item->tag;
            job.attempt = 0;

            const PollId pollId = item->id;
            job.onDone = [this, pollId](bool success, const ModbusResult& result, const std::string& error) {
                processPollCallback(pollId, success, result, error);
            };

            enqueueRead(job);
        }
    }

    void PLCController::ioLoop() {
        auto lastPollTick = SteadyClock::now();

        while (workerRunning_) {
            if (state_.load() == ConnState::Error || state_.load() == ConnState::Disconnected) {
                LOG_INFO("[PLC] Connection error or disconnected" + std::string(state_.load() == ConnState::Error ? "(error)" : "(disconnected)"));
                 while (workerRunning_) {

                    if (reconnect()) {
                        break;
                    }

                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(reconnectDelayMs_));
                }

                continue;
            }

            if (state_.load() != ConnState::Connected) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            if (!handleHeartbeatIfDue()) {
                setState(ConnState::Error);
                clearWriteQueue();
                notifyConnectionLost();
                continue;
            }

            const auto now = SteadyClock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPollTick).count() >= pollTickMs_) {
                processPollSchedule();
                lastPollTick = now;
            }

            ReadJob readJob;
            WriteJob writeJob;
            bool hasRead = false;
            bool hasWrite = false;

            if (writeQueue_.try_pop(writeJob)) {
                hasWrite = true;
            } else if (readQueue_.try_pop(readJob)) {
                hasRead = true;
            } else if (writeQueue_.pop_for(writeJob, std::chrono::milliseconds(5))) {
                hasWrite = true;
            } else if (readQueue_.pop_for(readJob, std::chrono::milliseconds(5))) {
                hasRead = true;
            }

            if (hasWrite) {
                std::string err;
                const bool ok = doWrite(writeJob, err);

                if (!ok) {
                    consecutiveErrors_++;
                    const bool severe =
                        (errno == ETIMEDOUT) ||
                        (errno == ECONNRESET) ||
                        (errno == EPIPE) ||
                        (errno == ENOTCONN) ||
                        (errno == ECONNREFUSED);

                    if (severe || consecutiveErrors_ >= maxConsecutiveErrors_) {
                        setState(ConnState::Error);
                        clearWriteQueue();
                        notifyConnectionLost();
                    }

                    if (writeJob.attempt < retries_) {
                        auto retry = writeJob;
                        retry.attempt++;
                        enqueueWrite(retry);
                    } else if (writeJob.onDone) {
                        writeJob.onDone(false, err);
                    }
                } else {
                    consecutiveErrors_ = 0;
                    if (writeJob.onDone) {
                        writeJob.onDone(true, "");
                    }
                }

                if (writeGapMs_ > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(writeGapMs_));
                }
                continue;
            }

            if (hasRead) {
                ModbusResult out;
                std::string err;
                const bool ok = doRead(readJob, out, err);

                if (!ok) {
                    consecutiveErrors_++;
                    const bool severe =
                        (errno == ETIMEDOUT) ||
                        (errno == ECONNRESET) ||
                        (errno == EPIPE) ||
                        (errno == ENOTCONN) ||
                        (errno == ECONNREFUSED);

                    if (severe || consecutiveErrors_ >= maxConsecutiveErrors_) {
                        setState(ConnState::Error);
                        notifyConnectionLost();
                    }

                    if (readJob.attempt < retries_) {
                        auto retry = readJob;
                        retry.attempt++;
                        enqueueRead(retry);
                    } else if (readJob.onDone) {
                        readJob.onDone(false, ModbusResult{}, err);
                    }
                } else {
                    consecutiveErrors_ = 0;
                    if (readJob.onDone) {
                        readJob.onDone(true, out, "");
                    }
                }

                if (readGapMs_ > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(readGapMs_));
                }
                continue;
            }
        }
    }
}
