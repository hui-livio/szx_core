#pragma once

#include "Result.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace SZX::ADMIN::TELEMETRY
{
struct BehaviorEventRecord final
{
    std::string id;
    std::string user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string ip;
    std::string user_agent;
    std::string details_json;
    std::string created_at;
};

enum class BatchWriteOutcome : unsigned char
{
    ReadyToCommit,
    AlreadyCommitted,
    RetryableFailure,
    CommitResultUnknown,
    RecordPermanentFailure,
    SystemicPermanentFailure,
    Committed,
};

struct BehaviorEventFilter final
{
    std::string from;
    std::string to;
    std::string eventName;
    std::string result;
    std::string userId;
    std::uint32_t limit{20};
    std::uint64_t offset{};
    std::string userSearch;
    std::string eventSearch;
};

struct BehaviorEventRow final
{
    std::string id;
    std::string user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string details_json;
    std::string created_at;
    std::string persisted_at;
    std::string username;
};

struct BehaviorEventPage final
{
    std::vector<BehaviorEventRow> items;
    std::uint64_t total{};
};

class BehaviorEventRepository
{
public:
    virtual ~BehaviorEventRepository() = default;

    /** Writes the telemetry table in the caller's transaction. */
    virtual auto appendBatch(std::span<const BehaviorEventRecord> records) -> SZX::COMMON::Result<BatchWriteOutcome> = 0;
    virtual auto probe() -> SZX::COMMON::Result<void>
    {
        return SZX::COMMON::Result<void>::Success();
    }
    virtual auto query(const BehaviorEventFilter &filter) -> SZX::COMMON::Result<BehaviorEventPage> = 0;
};
} // namespace SZX::ADMIN::TELEMETRY
