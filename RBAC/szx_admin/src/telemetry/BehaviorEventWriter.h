#pragma once

#include "BehaviorEventRepository.h"

#include "BlockQueue.hpp"
#include "Result.hpp"
#include "Serializer.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace SZX::ADMIN::INFRA
{
class DatabaseConnection;
class DatabasePool;
} // namespace SZX::ADMIN::INFRA

namespace SZX::ADMIN::TELEMETRY
{
struct TelemetryConfig final
{
    std::size_t queueCapacity{10000};
    std::size_t batchSize{200};
    std::uint32_t flushIntervalMs{200};
    std::uint32_t minCommitIntervalMs{200};
    std::uint32_t retryInitialMs{500};
    std::uint32_t retryMaxMs{10000};
    std::uint32_t databaseOperationTimeoutMs{5000};
    std::uint32_t shutdownFlushTimeoutMs{30000};
    std::uint32_t retryAfterSeconds{1};

    [[nodiscard]] auto validate() const -> SZX::COMMON::Result<void>;
};
SZX_REFL(TelemetryConfig, queueCapacity, batchSize, flushIntervalMs, minCommitIntervalMs, retryInitialMs, retryMaxMs,
         databaseOperationTimeoutMs, shutdownFlushTimeoutMs, retryAfterSeconds);

enum class AdmissionState : unsigned char
{
    Stopped,
    Accepting,
    Faulted,
    Draining,
};

struct DrainResult final
{
    bool clean{};
    std::size_t discarded{};
    std::size_t pendingCommitUnknown{};
    std::size_t remainingQueued{};
    std::size_t remainingBatch{};
};

class BehaviorEventWriter final
{
public:
    using ConnectionCallback = std::function<SZX::COMMON::Result<void>(INFRA::DatabaseConnection &)>;
    using ConnectionExecutor =
        std::function<SZX::COMMON::Result<void>(const ConnectionCallback &, std::chrono::steady_clock::time_point)>;

    BehaviorEventWriter(INFRA::DatabasePool &database, TelemetryConfig config);
    BehaviorEventWriter(ConnectionExecutor execute, TelemetryConfig config);
    ~BehaviorEventWriter();

    BehaviorEventWriter(const BehaviorEventWriter &)                     = delete;
    auto operator=(const BehaviorEventWriter &) -> BehaviorEventWriter & = delete;

    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto enqueue(BehaviorEventRecord record) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto stop() -> DrainResult;
    [[nodiscard]] auto resumeAfterFault() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto state() const noexcept -> AdmissionState;
    [[nodiscard]] auto queueDepth() const -> std::size_t;
    [[nodiscard]] auto highWaterMark() const -> std::size_t;
    [[nodiscard]] auto config() const noexcept -> const TelemetryConfig &;

private:
    using Clock = std::chrono::steady_clock;

    void run() noexcept;
    auto persist(std::span<const BehaviorEventRecord> records) -> BatchWriteOutcome;
    auto persistOrSplit(std::span<const BehaviorEventRecord> records) -> bool;
    auto waitUntil(Clock::time_point deadline) -> bool;
    void enterFaulted() noexcept;

    ConnectionExecutor execute_;
    TelemetryConfig config_;
    SZX::COMMON::BlockingQueue<BehaviorEventRecord> queue_;
    mutable std::mutex stateMutex_;
    std::condition_variable stateCondition_;
    std::atomic<AdmissionState> state_{AdmissionState::Stopped};
    std::thread worker_;
    std::optional<Clock::time_point> drainDeadline_;
    Clock::time_point lastCommit_{};
    std::atomic<std::size_t> discarded_{};
    std::atomic<std::size_t> pendingCommitUnknown_{};
    std::atomic<std::size_t> activeBatchSize_{};
    bool startedOnce_{};
};
} // namespace SZX::ADMIN::TELEMETRY
