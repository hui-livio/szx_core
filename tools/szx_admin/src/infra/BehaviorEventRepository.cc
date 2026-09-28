#include "BehaviorEventRepository.h"

#include "JsonHelper.hpp"
#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "UUID.hpp"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace SZX::ADMIN::TELEMETRY
{
inline constexpr auto get_alias_struct_name(BehaviorEventRecord *)
{
    return std::string_view{"client_behavior_events"};
}
YLT_REFL(BehaviorEventRecord, id, user_id, event_name, result, request_id, ip, user_agent, details_json, created_at);
} // namespace SZX::ADMIN::TELEMETRY

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
using SZX::COMMON::Result;
using TELEMETRY::BatchWriteOutcome;

struct BehaviorPageRecord final
{
    std::string id;
    std::string user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string details_json;
    std::string created_at;
    std::string persisted_at;
    std::string login_identifier;
};
YLT_REFL(BehaviorPageRecord, id, user_id, event_name, result, request_id, details_json, created_at, persisted_at, login_identifier);

template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto escapeLike(std::string_view value) -> std::string
{
    std::string result;
    result.reserve(value.size() + 2);
    for (const auto character : value)
    {
        if (character == '\\' || character == '%' || character == '_')
            result.push_back('\\');
        result.push_back(character);
    }
    return result;
}

auto validRecord(const TELEMETRY::BehaviorEventRecord &record) -> bool
{
    if (!SZX::COMMON::Uuid::parse(record.id) || !SZX::COMMON::Uuid::parse(record.user_id) || record.event_name.empty() ||
        record.event_name.size() > 96 || (record.result != "success" && record.result != "failure") ||
        record.request_id.empty() || record.request_id.size() > 128 || record.ip.empty() || record.ip.size() > 45 ||
        record.user_agent.size() > 512 || record.details_json.size() > 4096 || record.created_at.empty())
        return false;
    return SZX::COMMON::JsonHelper::ParseRequiredObject(record.details_json).has_value();
}

auto sameJson(const std::string &lhs, const std::string &rhs) -> bool
{
    const auto left  = SZX::COMMON::JsonHelper::ParseRequiredObject(lhs);
    const auto right = SZX::COMMON::JsonHelper::ParseRequiredObject(rhs);
    return left && right && *left == *right;
}

auto sameRecord(const TELEMETRY::BehaviorEventRecord &lhs, const TELEMETRY::BehaviorEventRecord &rhs) -> bool
{
    return lhs.id == rhs.id && lhs.user_id == rhs.user_id && lhs.event_name == rhs.event_name && lhs.result == rhs.result &&
        lhs.request_id == rhs.request_id && lhs.ip == rhs.ip && lhs.user_agent == rhs.user_agent &&
        sameJson(lhs.details_json, rhs.details_json) && lhs.created_at == rhs.created_at;
}

auto failureOutcome(const OrmConnection &connection) -> BatchWriteOutcome
{
    auto diagnostic = connection.lastError();
    std::ranges::transform(diagnostic, diagnostic.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    const auto contains = [&](std::string_view text)
    {
        return diagnostic.find(text) != std::string::npos;
    };
    if (contains("access denied") || contains("command denied") || contains("unknown database") || contains("unknown column") ||
        contains("doesn't exist") || contains("does not exist"))
        return BatchWriteOutcome::SystemicPermanentFailure;
    if (contains("duplicate entry") || contains("data truncated") || contains("incorrect string value") ||
        contains("data too long") || contains("invalid json") || contains("check constraint"))
        return BatchWriteOutcome::RecordPermanentFailure;
    return BatchWriteOutcome::RetryableFailure;
}
} // namespace

auto MySqlBehaviorEventRepository::appendBatch(std::span<const TELEMETRY::BehaviorEventRecord> records)
    -> Result<BatchWriteOutcome>
{
    try
    {
        if (records.empty())
            return Result<BatchWriteOutcome>::Success(BatchWriteOutcome::ReadyToCommit);
        if (std::any_of(records.begin(), records.end(), [](const auto &record) { return !validRecord(record); }))
            return Result<BatchWriteOutcome>::Success(BatchWriteOutcome::RecordPermanentFailure);

        if (records.size() == 1)
        {
            const auto &record = records.front();
            const auto current = connection_.query_s<TELEMETRY::BehaviorEventRecord>("id = ?", record.id);
            if (connection_.has_error())
                return Result<BatchWriteOutcome>::Success(failureOutcome(connection_));
            if (!current.empty())
            {
                const auto identical = current.size() == 1 && sameRecord(current.front(), record);
                return Result<BatchWriteOutcome>::Success(identical ? BatchWriteOutcome::AlreadyCommitted :
                                                                      BatchWriteOutcome::RecordPermanentFailure);
            }
        }

        std::vector<TELEMETRY::BehaviorEventRecord> current(records.begin(), records.end());
        if (connection_.insert(current) != static_cast<int>(current.size()) || connection_.has_error())
            return Result<BatchWriteOutcome>::Success(failureOutcome(connection_));
        return Result<BatchWriteOutcome>::Success(BatchWriteOutcome::ReadyToCommit);
    } catch (...)
    {
        return failed<BatchWriteOutcome>(ErrorCode::DatabaseQueryFailed);
    }
}

auto MySqlBehaviorEventRepository::probe() -> Result<void>
{
    try
    {
        if (!connection_.execute("SAVEPOINT szx_behavior_probe") || !connection_.execute("SET @szx_behavior_probe_id = UUID()") ||
            !connection_.execute(
                "INSERT INTO client_behavior_events "
                "(id,user_id,event_name,result,request_id,ip,user_agent,details_json,created_at) "
                "SELECT @szx_behavior_probe_id,id,'system.probe','success','writer-resume-probe',NULL,NULL,JSON_OBJECT(),"
                "CURRENT_TIMESTAMP(6) FROM users ORDER BY id LIMIT 1"))
        {
            (void)connection_.execute("ROLLBACK TO SAVEPOINT szx_behavior_probe");
            (void)connection_.execute("RELEASE SAVEPOINT szx_behavior_probe");
            return failed<void>(ErrorCode::DatabaseQueryFailed);
        }
        const auto rolledBack = connection_.execute("ROLLBACK TO SAVEPOINT szx_behavior_probe");
        const auto released   = connection_.execute("RELEASE SAVEPOINT szx_behavior_probe");
        return rolledBack && released && !connection_.has_error() ? Result<void>::Success() :
                                                                    failed<void>(ErrorCode::DatabaseQueryFailed);
    } catch (...)
    {
        return failed<void>(ErrorCode::DatabaseQueryFailed);
    }
}

auto MySqlBehaviorEventRepository::query(const TELEMETRY::BehaviorEventFilter &filter) -> Result<TELEMETRY::BehaviorEventPage>
{
    try
    {
        if (filter.limit == 0)
            return failed<TELEMETRY::BehaviorEventPage>(ErrorCode::InternalFailure);
        const auto userPattern  = "%" + escapeLike(filter.userSearch) + "%";
        const auto eventPattern = "%" + escapeLike(filter.eventSearch) + "%";
        const std::string source{" FROM client_behavior_events e LEFT JOIN users u ON u.id = e.user_id WHERE "};
        const std::string predicate{"(? = '' OR e.created_at >= ?) AND (? = '' OR e.created_at <= ?) AND "
                                    "(? = '' OR e.event_name = ?) AND (? = '' OR e.result = ?) AND "
                                    "(? = '' OR e.user_id = ?) AND "
                                    "(? = '' OR u.display_name LIKE ? OR u.login_identifier LIKE ?) AND "
                                    "(? = '' OR e.event_name LIKE ?)"};
        const auto totals = connection_.query_s<std::tuple<std::uint64_t>>(
            "SELECT COUNT(*)" + source + predicate, filter.from, filter.from, filter.to, filter.to, filter.eventName,
            filter.eventName, filter.result, filter.result, filter.userId, filter.userId, filter.userSearch, userPattern,
            userPattern, userPattern, filter.eventSearch, eventPattern);
        if (connection_.has_error() || totals.size() != 1)
            return failed<TELEMETRY::BehaviorEventPage>(ErrorCode::DatabaseQueryFailed);

        TELEMETRY::BehaviorEventPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total <= filter.offset)
            return Result<TELEMETRY::BehaviorEventPage>::Success(std::move(page));
        auto rows = connection_.query_s<BehaviorPageRecord>(
            // Keep the JSON projection bounded.  Without an explicit CHAR
            // length MySQL advertises a 4 GiB result and ormpp allocates that
            // size for every page fetch.
            "SELECT e.id, e.user_id, e.event_name, e.result, e.request_id, CAST(e.details_json AS CHAR(1048576)), "
            "e.created_at, e.persisted_at, COALESCE(u.login_identifier, '')" +
                source + predicate + " ORDER BY e.created_at DESC, e.id ASC LIMIT ? OFFSET ?",
            filter.from, filter.from, filter.to, filter.to, filter.eventName, filter.eventName, filter.result, filter.result,
            filter.userId, filter.userId, filter.userSearch, userPattern, userPattern, userPattern, filter.eventSearch,
            eventPattern, static_cast<std::uint64_t>(filter.limit), filter.offset);
        if (connection_.has_error())
            return failed<TELEMETRY::BehaviorEventPage>(ErrorCode::DatabaseQueryFailed);

        page.items.reserve(rows.size());
        for (auto &row : rows)
        {
            page.items.push_back({.id           = std::move(row.id),
                                  .user_id      = std::move(row.user_id),
                                  .event_name   = std::move(row.event_name),
                                  .result       = std::move(row.result),
                                  .request_id   = std::move(row.request_id),
                                  .details_json = std::move(row.details_json),
                                  .created_at   = std::move(row.created_at),
                                  .persisted_at = std::move(row.persisted_at),
                                  .login_identifier     = std::move(row.login_identifier)});
        }
        return Result<TELEMETRY::BehaviorEventPage>::Success(std::move(page));
    } catch (...)
    {
        return failed<TELEMETRY::BehaviorEventPage>(ErrorCode::DatabaseQueryFailed);
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
