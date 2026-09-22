#include "AuditRepository.h"

#include "JsonHelper.hpp"
#include "Logger.h"
#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace SZX::ADMIN::AUDIT
{
inline constexpr auto get_alias_struct_name(AuditRecord *)
{
    return std::string_view{"security_audit_logs"};
}
YLT_REFL(AuditRecord, id, actor_user_id, target_user_id, event_name, result, request_id, ip, user_agent, details_json,
         created_at);
} // namespace SZX::ADMIN::AUDIT

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
using SZX::COMMON::Result;

struct LegacyAuditRecord final
{
    std::string id;
    std::optional<std::string> actor_user_id;
    std::optional<std::string> target_user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string ip;
    std::string user_agent;
    std::string details_json;
    std::string created_at;
};
inline constexpr auto get_alias_struct_name(LegacyAuditRecord *)
{
    return std::string_view{"user_logs"};
}
YLT_REFL(LegacyAuditRecord, id, actor_user_id, target_user_id, event_name, result, request_id, ip, user_agent, details_json,
         created_at);

struct AuditPageRecord final
{
    std::string id;
    std::optional<std::string> actor_user_id;
    std::optional<std::string> target_user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::uint64_t details_size{};
    std::string created_at;
    std::string actor_username;
    std::string source;
    std::string storage_table;
};
YLT_REFL(AuditPageRecord, id, actor_user_id, target_user_id, event_name, result, request_id, details_size, created_at,
         actor_username, source, storage_table);

template <typename T>
auto queryFailed() -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(ErrorCode::DatabaseQueryFailed), {});
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

void reportAuditState(bool available) noexcept
{
    static std::atomic_bool unavailable{};
    if (unavailable.exchange(!available) == !available)
        return;
    try
    {
        LOGM_INFO("szx_admin.audit", "event={}", available ? "write_recovered" : "write_failed");
    } catch (...)
    {
    }
}

auto acceptedAt() -> std::string
{
    const auto now = SZX::COMMON::TimeHelper::NowTimestamp();
    std::ostringstream stream;
    stream << SZX::COMMON::TimeHelper::TimestampToString(now) << '.' << std::setw(3) << std::setfill('0') << now % 1000
           << "000";
    return stream.str();
}
} // namespace

auto MySqlAuditRepository::append(const AUDIT::AuditRecord &record) -> Result<void>
{
    try
    {
        auto stored = record;
        if (stored.created_at.empty())
            stored.created_at = acceptedAt();
        const auto trusted = connection_.insert(stored) == 1 && !connection_.has_error();
        if (!trusted)
        {
            reportAuditState(false);
            return Result<void>::Failure(static_cast<int>(ErrorCode::AuditWriteFailed), {});
        }
        if (!dualWriteLegacy_)
        {
            reportAuditState(true);
            return Result<void>::Success();
        }
        const LegacyAuditRecord legacy{stored.id,          stored.actor_user_id, stored.target_user_id,
                                       stored.event_name,  stored.result,        stored.request_id,
                                       stored.ip,          stored.user_agent,    stored.details_json,
                                       stored.created_at};
        const auto compatible = connection_.insert(legacy) == 1 && !connection_.has_error();
        reportAuditState(compatible);
        return compatible ? Result<void>::Success() :
                            Result<void>::Failure(static_cast<int>(ErrorCode::AuditWriteFailed), {});
    } catch (...)
    {
        reportAuditState(false);
        return Result<void>::Failure(static_cast<int>(ErrorCode::AuditWriteFailed), {});
    }
}

auto MySqlAuditRepository::query(const AUDIT::AuditFilter &filter) -> Result<AUDIT::AuditPage>
{
    return queryTable(filter, "security_audit_logs");
}

auto MySqlAuditRepository::queryLegacy(const AUDIT::AuditFilter &filter) -> Result<AUDIT::AuditPage>
{
    return queryCompatibility(filter);
}

auto MySqlAuditRepository::queryCompatibility(const AUDIT::AuditFilter &filter) -> Result<AUDIT::AuditPage>
{
    try
    {
        if (filter.limit == 0)
            return Result<AUDIT::AuditPage>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        const auto pattern      = "%" + escapeLike(filter.actorSearch) + "%";
        const auto eventPattern = "%" + escapeLike(filter.eventSearch) + "%";
        const auto eventNames   = SZX::COMMON::Json(filter.eventNames).dump();
        const std::string legacyUnion = readLegacy_ ?
            "UNION ALL "
            "SELECT x.id, x.actor_user_id, x.target_user_id, x.event_name, x.result, x.request_id, x.details_json, "
            "x.created_at, CASE WHEN x.event_name LIKE 'auth.%' OR x.event_name LIKE 'user.%' THEN 'security_audit' "
            "WHEN x.actor_user_id IS NOT NULL AND x.actor_user_id = x.target_user_id "
            "AND x.result IN ('success','failure') THEN 'client_behavior' ELSE 'legacy_unknown' END AS source, "
            "'user_logs' AS storage_table "
            "FROM user_logs x WHERE NOT EXISTS (SELECT 1 FROM security_audit_logs a3 WHERE a3.id = x.id) "
            "AND NOT EXISTS (SELECT 1 FROM client_behavior_events b3 WHERE b3.id = x.id)" :
            std::string{};
        const std::string unionSql =
            "(SELECT a.id, a.actor_user_id, a.target_user_id, a.event_name, a.result, a.request_id, a.details_json, "
            "a.created_at, 'security_audit' AS source, 'security_audit_logs' AS storage_table "
            "FROM security_audit_logs a "
            "UNION ALL "
            "SELECT b.id, b.user_id, b.user_id, b.event_name, b.result, b.request_id, b.details_json, b.created_at, "
            "'client_behavior' AS source, 'client_behavior_events' AS storage_table FROM client_behavior_events b "
            "WHERE NOT EXISTS (SELECT 1 FROM security_audit_logs a2 WHERE a2.id = b.id) "
            + legacyUnion + ")";
        const std::string source = " FROM " + unionSql + " l LEFT JOIN users u ON u.id = l.actor_user_id WHERE ";
        const std::string predicate =
            "(? = '' OR l.created_at >= ?) AND (? = '' OR l.created_at <= ?) AND "
            "(? = '' OR l.event_name = ?) AND (? = '' OR l.result = ?) AND "
            "(? = '' OR l.actor_user_id = ?) AND "
            "(? = '' OR u.display_name LIKE ? OR u.username LIKE ? OR u.uid LIKE ?) AND "
            "(? = '' OR l.event_name LIKE ? OR JSON_CONTAINS(?, JSON_QUOTE(l.event_name)))";
        const auto totals = connection_.query_s<std::tuple<std::uint64_t>>(
            "SELECT COUNT(*)" + source + predicate, filter.from, filter.from, filter.to, filter.to, filter.eventName,
            filter.eventName, filter.result, filter.result, filter.actorUserId, filter.actorUserId, filter.actorSearch,
            pattern, pattern, pattern, filter.eventSearch, eventPattern, eventNames);
        if (connection_.has_error() || totals.size() != 1)
            return queryFailed<AUDIT::AuditPage>();

        AUDIT::AuditPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total <= filter.offset)
            return Result<AUDIT::AuditPage>::Success(std::move(page));
        auto rows = connection_.query_s<AuditPageRecord>(
            "SELECT l.id, l.actor_user_id, l.target_user_id, l.event_name, l.result, l.request_id, "
            "OCTET_LENGTH(l.details_json), l.created_at, COALESCE(u.username, u.uid, ''), l.source, l.storage_table" +
                source + predicate + " ORDER BY l.created_at DESC, l.id ASC LIMIT ? OFFSET ?",
            filter.from, filter.from, filter.to, filter.to, filter.eventName, filter.eventName, filter.result, filter.result,
            filter.actorUserId, filter.actorUserId, filter.actorSearch, pattern, pattern, pattern, filter.eventSearch,
            eventPattern, eventNames, static_cast<std::uint64_t>(filter.limit), filter.offset);
        if (connection_.has_error())
            return queryFailed<AUDIT::AuditPage>();
        page.items.reserve(rows.size());
        for (auto &row : rows)
        {
            constexpr std::uint64_t chunkBytes = 16384;
            std::string details;
            for (std::uint64_t byteOffset = 0; byteOffset < row.details_size; byteOffset += chunkBytes)
            {
                if (row.storage_table != "security_audit_logs" && row.storage_table != "client_behavior_events" &&
                    row.storage_table != "user_logs")
                    return queryFailed<AUDIT::AuditPage>();
                const auto chunks = connection_.query_s<std::tuple<std::string>>(
                    "SELECT CAST(SUBSTRING(CAST(details_json AS BINARY), ?, 16384) "
                    "AS CHAR(16384) CHARACTER SET binary) FROM " +
                        row.storage_table + " WHERE id = ?",
                    byteOffset + 1, row.id);
                if (connection_.has_error() || chunks.size() != 1 ||
                    std::get<0>(chunks.front()).size() != (std::min)(chunkBytes, row.details_size - byteOffset))
                    return queryFailed<AUDIT::AuditPage>();
                details += std::get<0>(chunks.front());
            }
            page.items.push_back({.id             = std::move(row.id),
                                  .actor_user_id  = std::move(row.actor_user_id),
                                  .target_user_id = std::move(row.target_user_id),
                                  .event_name     = std::move(row.event_name),
                                  .result         = std::move(row.result),
                                  .request_id     = std::move(row.request_id),
                                  .details_json   = std::move(details),
                                  .created_at     = std::move(row.created_at),
                                  .actor_username = std::move(row.actor_username),
                                  .source         = std::move(row.source)});
        }
        return Result<AUDIT::AuditPage>::Success(std::move(page));
    } catch (...)
    {
        return queryFailed<AUDIT::AuditPage>();
    }
}

auto MySqlAuditRepository::queryTable(const AUDIT::AuditFilter &filter, std::string_view table)
    -> Result<AUDIT::AuditPage>
{
    try
    {
        if (filter.limit == 0 || (table != "security_audit_logs" && table != "user_logs"))
            return Result<AUDIT::AuditPage>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        const auto from         = filter.from;
        const auto to           = filter.to;
        const auto event        = filter.eventName;
        const auto result       = filter.result;
        const auto actor        = filter.actorUserId;
        const auto username     = filter.actorSearch;
        const auto pattern      = "%" + escapeLike(username) + "%";
        const auto eventSearch  = filter.eventSearch;
        const auto eventPattern = "%" + escapeLike(eventSearch) + "%";
        const auto eventNames   = SZX::COMMON::Json(filter.eventNames).dump();
        const auto limit        = static_cast<std::uint64_t>(filter.limit);
        const auto offset       = filter.offset;
        const std::string source = " FROM " + std::string{table} + " l LEFT JOIN users u ON u.id = l.actor_user_id WHERE ";
        const std::string predicate{"(? = '' OR l.created_at >= ?) AND (? = '' OR l.created_at <= ?) AND "
                                    "(? = '' OR l.event_name = ?) AND (? = '' OR l.result = ?) AND "
                                    "(? = '' OR l.actor_user_id = ?) AND "
                                    "(? = '' OR u.display_name LIKE ? OR u.username LIKE ? OR u.uid LIKE ?) AND "
                                    "(? = '' OR l.event_name LIKE ? OR JSON_CONTAINS(?, JSON_QUOTE(l.event_name)))"};

        const auto totals = connection_.query_s<std::tuple<std::uint64_t>>(
            "SELECT COUNT(*)" + source + predicate, from, from, to, to, event, event, result, result, actor, actor, username,
            pattern, pattern, pattern, eventSearch, eventPattern, eventNames);
        if (connection_.has_error() || totals.size() != 1)
            return queryFailed<AUDIT::AuditPage>();

        AUDIT::AuditPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total <= offset)
            return Result<AUDIT::AuditPage>::Success(std::move(page));

        auto rows = connection_.query_s<AuditPageRecord>(
            "SELECT l.id, l.actor_user_id, l.target_user_id, l.event_name, l.result, l.request_id, "
            "OCTET_LENGTH(l.details_json), l.created_at, COALESCE(u.username, u.uid, ''), " +
                std::string{table == "security_audit_logs" ? "'security_audit'" : "'legacy_unknown'"} +
                ", '" + std::string{table} + "'" +
                source + predicate + " ORDER BY l.created_at DESC, l.id ASC LIMIT ? OFFSET ?",
            from, from, to, to, event, event, result, result, actor, actor, username, pattern, pattern, pattern, eventSearch,
            eventPattern, eventNames, limit, offset);
        if (connection_.has_error())
            return queryFailed<AUDIT::AuditPage>();
        page.items.reserve(rows.size());
        for (auto &row : rows)
        {
            constexpr std::uint64_t chunkBytes = 16384;
            std::string details;
            for (std::uint64_t byteOffset = 0; byteOffset < row.details_size; byteOffset += chunkBytes)
            {
                const auto chunks = connection_.query_s<std::tuple<std::string>>(
                    "SELECT CAST(SUBSTRING(CAST(details_json AS BINARY), ?, 16384) "
                    "AS CHAR(16384) CHARACTER SET binary) FROM " +
                        std::string{table} + " WHERE id = ?",
                    byteOffset + 1, row.id);
                if (connection_.has_error() || chunks.size() != 1 ||
                    std::get<0>(chunks.front()).size() != (std::min)(chunkBytes, row.details_size - byteOffset))
                    return queryFailed<AUDIT::AuditPage>();
                details += std::get<0>(chunks.front());
            }
            page.items.push_back({.id             = std::move(row.id),
                                  .actor_user_id  = std::move(row.actor_user_id),
                                  .target_user_id = std::move(row.target_user_id),
                                  .event_name     = std::move(row.event_name),
                                  .result         = std::move(row.result),
                                  .request_id     = std::move(row.request_id),
                                  .details_json   = std::move(details),
                                  .created_at     = std::move(row.created_at),
                                  .actor_username = std::move(row.actor_username),
                                  .source         = std::move(row.source)});
        }
        return Result<AUDIT::AuditPage>::Success(std::move(page));
    } catch (...)
    {
        return queryFailed<AUDIT::AuditPage>();
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
