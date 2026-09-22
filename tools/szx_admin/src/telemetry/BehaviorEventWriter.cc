#include "BehaviorEventWriter.h"

#include "Logger.h"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <algorithm>

namespace SZX::ADMIN::TELEMETRY
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;

auto failure(ErrorCode code) -> Result<void>
{
    return Result<void>::Failure(static_cast<int>(code), {});
}
} // namespace

auto TelemetryConfig::validate() const -> Result<void>
{
    if (queueCapacity == 0 || batchSize == 0 || flushIntervalMs == 0 || minCommitIntervalMs == 0 || retryInitialMs == 0 ||
        retryMaxMs == 0 || databaseOperationTimeoutMs == 0 || shutdownFlushTimeoutMs == 0 || retryAfterSeconds == 0 ||
        batchSize > queueCapacity || retryInitialMs > retryMaxMs || minCommitIntervalMs > flushIntervalMs ||
        databaseOperationTimeoutMs > shutdownFlushTimeoutMs)
        return failure(ErrorCode::InvalidConfiguration);
    return Result<void>::Success();
}

BehaviorEventWriter::BehaviorEventWriter(INFRA::DatabasePool &database, TelemetryConfig config) :
    BehaviorEventWriter([&database](const ConnectionCallback &callback, Clock::time_point deadline)
                        { return database.withConnectionUntil(callback, deadline); }, std::move(config))
{
}

BehaviorEventWriter::BehaviorEventWriter(ConnectionExecutor execute, TelemetryConfig config) :
    execute_(std::move(execute)),
    config_(std::move(config)),
    queue_(config_.queueCapacity)
{
}

BehaviorEventWriter::~BehaviorEventWriter()
{
    (void)stop();
}

auto BehaviorEventWriter::start() -> Result<void>
{
    const auto valid = config_.validate();
    if (!valid.ok)
        return valid;
    if (!execute_)
        return failure(ErrorCode::InvalidConfiguration);
    std::lock_guard lock(stateMutex_);
    if (state_.load(std::memory_order_acquire) == AdmissionState::Accepting)
        return Result<void>::Success();
    if (startedOnce_ || queue_.is_closed())
        return failure(ErrorCode::BehaviorEventQueueClosed);
    try
    {
        state_.store(AdmissionState::Accepting, std::memory_order_release);
        startedOnce_ = true;
        worker_      = std::thread([this] { run(); });
        return Result<void>::Success();
    } catch (...)
    {
        state_.store(AdmissionState::Stopped, std::memory_order_release);
        return failure(ErrorCode::ServiceUnavailable);
    }
}

auto BehaviorEventWriter::enqueue(BehaviorEventRecord record) -> Result<void>
{
    std::lock_guard lock(stateMutex_);
    if (state_.load(std::memory_order_acquire) != AdmissionState::Accepting)
        return failure(ErrorCode::BehaviorEventQueueClosed);
    switch (queue_.try_push(std::move(record)))
    {
    case SZX::COMMON::QueuePushResult::Pushed:
        return Result<void>::Success();
    case SZX::COMMON::QueuePushResult::Full:
        return failure(ErrorCode::BehaviorEventQueueFull);
    case SZX::COMMON::QueuePushResult::Closed:
        return failure(ErrorCode::BehaviorEventQueueClosed);
    }
    return failure(ErrorCode::InternalFailure);
}

auto BehaviorEventWriter::stop() -> DrainResult
{
    {
        std::lock_guard lock(stateMutex_);
        const auto current = state_.load(std::memory_order_acquire);
        if (current != AdmissionState::Stopped && current != AdmissionState::Draining)
        {
            state_.store(AdmissionState::Draining, std::memory_order_release);
            drainDeadline_ = Clock::now() + std::chrono::milliseconds{config_.shutdownFlushTimeoutMs};
            queue_.close();
            stateCondition_.notify_all();
        }
    }
    if (worker_.joinable())
        worker_.join();
    const auto remaining = queue_.size();
    if (remaining != 0)
        queue_.clear();
    std::lock_guard lock(stateMutex_);
    discarded_.fetch_add(remaining, std::memory_order_relaxed);
    state_.store(AdmissionState::Stopped, std::memory_order_release);
    const auto discarded = discarded_.load(std::memory_order_relaxed);
    const auto pending   = pendingCommitUnknown_.load(std::memory_order_relaxed);
    const auto active    = activeBatchSize_.load(std::memory_order_relaxed);
    return {.clean                = discarded == 0 && pending == 0 && remaining == 0 && active == 0,
            .discarded            = discarded,
            .pendingCommitUnknown = pending,
            .remainingQueued      = remaining,
            .remainingBatch       = active};
}

auto BehaviorEventWriter::resumeAfterFault() -> Result<void>
{
    {
        std::lock_guard lock(stateMutex_);
        if (state_.load(std::memory_order_acquire) != AdmissionState::Faulted)
            return failure(ErrorCode::BehaviorEventQueueClosed);
    }
    const auto deadline = Clock::now() + std::chrono::milliseconds{config_.databaseOperationTimeoutMs};
    const auto probed   = execute_(
        [](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            auto *repository = connection.behaviorEventRepository();
            if (!repository)
                return failure(ErrorCode::DatabaseQueryFailed);
            const auto begun = connection.begin();
            if (!begun.ok)
                return begun;
            const auto probe = repository->probe();
            const auto ended = connection.rollback();
            return probe.ok ? ended : probe;
        },
        deadline);
    if (!probed.ok)
        return probed;
    std::lock_guard lock(stateMutex_);
    if (state_.load(std::memory_order_acquire) != AdmissionState::Faulted)
        return failure(ErrorCode::BehaviorEventQueueClosed);
    state_.store(AdmissionState::Accepting, std::memory_order_release);
    stateCondition_.notify_all();
    return Result<void>::Success();
}

auto BehaviorEventWriter::state() const noexcept -> AdmissionState
{
    return state_.load(std::memory_order_acquire);
}

auto BehaviorEventWriter::queueDepth() const -> std::size_t
{
    return queue_.size();
}

auto BehaviorEventWriter::highWaterMark() const -> std::size_t
{
    return queue_.high_water_mark();
}

auto BehaviorEventWriter::config() const noexcept -> const TelemetryConfig &
{
    return config_;
}

auto BehaviorEventWriter::waitUntil(Clock::time_point deadline) -> bool
{
    std::unique_lock lock(stateMutex_);
    stateCondition_.wait_until(lock, deadline,
                               [this] { return state_.load(std::memory_order_acquire) == AdmissionState::Draining; });
    return state_.load(std::memory_order_acquire) != AdmissionState::Draining || !drainDeadline_ ||
        Clock::now() < *drainDeadline_;
}

auto BehaviorEventWriter::persist(std::span<const BehaviorEventRecord> records) -> BatchWriteOutcome
{
    BatchWriteOutcome outcome = BatchWriteOutcome::RetryableFailure;
    auto deadline             = Clock::now() + std::chrono::milliseconds{config_.databaseOperationTimeoutMs};
    {
        std::lock_guard lock(stateMutex_);
        if (drainDeadline_)
            deadline = (std::min)(deadline, *drainDeadline_);
    }
    if (Clock::now() >= deadline)
        return BatchWriteOutcome::RetryableFailure;
    const auto operation = execute_(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            auto *repository = connection.behaviorEventRepository();
            if (!repository)
            {
                outcome = BatchWriteOutcome::SystemicPermanentFailure;
                return Result<void>::Success();
            }
            const auto begun = connection.begin();
            if (!begun.ok)
                return begun;
            const auto written = repository->appendBatch(records);
            if (!written.ok)
            {
                (void)connection.rollback();
                return Result<void>::Failure(written.error, {});
            }
            outcome = written.value;
            if (outcome == BatchWriteOutcome::AlreadyCommitted)
            {
                (void)connection.rollback();
                lastCommit_ = Clock::now();
                return Result<void>::Success();
            }
            if (outcome != BatchWriteOutcome::ReadyToCommit)
            {
                (void)connection.rollback();
                return Result<void>::Success();
            }
            const auto committed = connection.commitWithOutcome();
            if (!committed.ok || committed.value == INFRA::CommitOutcome::Unknown)
            {
                connection.invalidate();
                outcome = BatchWriteOutcome::CommitResultUnknown;
                pendingCommitUnknown_.store(1, std::memory_order_relaxed);
                return Result<void>::Success();
            }
            if (committed.value == INFRA::CommitOutcome::NotCommitted)
            {
                outcome = BatchWriteOutcome::RetryableFailure;
                return Result<void>::Success();
            }
            lastCommit_ = Clock::now();
            outcome     = BatchWriteOutcome::Committed;
            return Result<void>::Success();
        },
        deadline);
    return operation.ok ? outcome : BatchWriteOutcome::RetryableFailure;
}

auto BehaviorEventWriter::persistOrSplit(std::span<const BehaviorEventRecord> records) -> bool
{
    auto retry = std::chrono::milliseconds{config_.retryInitialMs};
    for (;;)
    {
        if (lastCommit_ != Clock::time_point{})
        {
            const auto next = lastCommit_ + std::chrono::milliseconds{config_.minCommitIntervalMs};
            if (Clock::now() < next && !waitUntil(next))
                return false;
        }
        const auto outcome = persist(records);
        if (outcome == BatchWriteOutcome::Committed || outcome == BatchWriteOutcome::AlreadyCommitted)
        {
            pendingCommitUnknown_.store(0, std::memory_order_relaxed);
            return true;
        }
        if (outcome == BatchWriteOutcome::RecordPermanentFailure)
        {
            if (records.size() == 1)
            {
                discarded_.fetch_add(1, std::memory_order_relaxed);
                LOGM_WARN("szx_admin.telemetry", "event=record_discarded event_id={} reason=permanent_failure",
                          records.front().id);
                return true;
            }
            const auto middle = records.size() / 2;
            return persistOrSplit(records.first(middle)) && persistOrSplit(records.subspan(middle));
        }
        if (outcome == BatchWriteOutcome::SystemicPermanentFailure)
        {
            enterFaulted();
            std::unique_lock lock(stateMutex_);
            stateCondition_.wait(lock, [this] { return state_.load(std::memory_order_acquire) != AdmissionState::Faulted; });
            if (state_.load(std::memory_order_acquire) == AdmissionState::Draining)
                return false;
            continue;
        }
        const auto deadline = Clock::now() + retry;
        if (!waitUntil(deadline))
            return false;
        retry = (std::min)(retry * 2, std::chrono::milliseconds{config_.retryMaxMs});
    }
}

void BehaviorEventWriter::enterFaulted() noexcept
{
    std::lock_guard lock(stateMutex_);
    if (state_.load(std::memory_order_acquire) == AdmissionState::Accepting)
        state_.store(AdmissionState::Faulted, std::memory_order_release);
    try
    {
        LOGM_ERROR("szx_admin.telemetry", "event=writer_faulted");
    } catch (...)
    {
    }
}

void BehaviorEventWriter::run() noexcept
{
    for (;;)
    {
        try
        {
            for (;;)
            {
                BehaviorEventRecord first;
                if (!queue_.pop_for(first, std::chrono::milliseconds{config_.flushIntervalMs}))
                {
                    if (queue_.is_closed())
                        return;
                    continue;
                }
                std::vector<BehaviorEventRecord> batch;
                batch.reserve(config_.batchSize);
                batch.push_back(std::move(first));
                activeBatchSize_.store(batch.size(), std::memory_order_relaxed);
                const auto flushAt = Clock::now() + std::chrono::milliseconds{config_.flushIntervalMs};
                while (batch.size() < config_.batchSize)
                {
                    BehaviorEventRecord next;
                    const auto draining = state_.load(std::memory_order_acquire) == AdmissionState::Draining;
                    if (draining ? !queue_.try_pop(next) :
                                   !queue_.pop_for(next, (std::max)(Clock::duration::zero(), flushAt - Clock::now())))
                        break;
                    batch.push_back(std::move(next));
                    activeBatchSize_.store(batch.size(), std::memory_order_relaxed);
                }
                if (!persistOrSplit(batch))
                {
                    discarded_.fetch_add(batch.size(), std::memory_order_relaxed);
                    activeBatchSize_.store(0, std::memory_order_relaxed);
                    return;
                }
                activeBatchSize_.store(0, std::memory_order_relaxed);
            }
        } catch (...)
        {
            enterFaulted();
            std::unique_lock lock(stateMutex_);
            stateCondition_.wait(lock, [this] { return state_.load(std::memory_order_acquire) != AdmissionState::Faulted; });
            if (state_.load(std::memory_order_acquire) != AdmissionState::Accepting)
                return;
        }
    }
}
} // namespace SZX::ADMIN::TELEMETRY
