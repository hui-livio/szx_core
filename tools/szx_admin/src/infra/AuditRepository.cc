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

struct AuditPageRecord final
{
    std::string id;
    std::optional<std::string> actor_user_id;
    std::optional<std::string> target_user_id;
    std::string event_name;
    std::string result;
    std::string request_id;
    std::string details_json;
    std::string created_at;
    std::string actor_login_identifier;
    std::string storage_table;
};
YLT_REFL(AuditPageRecord, id, actor_user_id, target_user_id, event_name, result, request_id, details_json, created_at,
         actor_login_identifier, storage_table);

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
    stream << SZX::COMMON::TimeHelper::TimestampToString(now) << '.' << std::setw(3) << std::setfill('0') << now % 1000 << "000";
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
        reportAuditState(true);
        return Result<void>::Success();
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

auto MySqlAuditRepository::queryTable(const AUDIT::AuditFilter &filter, std::string_view table) -> Result<AUDIT::AuditPage>
{
    try
    {
        if (filter.limit == 0 || table != "security_audit_logs")
            return Result<AUDIT::AuditPage>::Failure(static_cast<int>(ErrorCode::InternalFailure), {});
        const auto from          = filter.from;
        const auto to            = filter.to;
        const auto event         = filter.eventName;
        const auto result        = filter.result;
        const auto actor         = filter.actorUserId;
        const auto username      = filter.actorSearch;
        const auto pattern       = "%" + escapeLike(username) + "%";
        const auto eventSearch   = filter.eventSearch;
        const auto eventPattern  = "%" + escapeLike(eventSearch) + "%";
        const auto eventNames    = SZX::COMMON::Json(filter.eventNames).dump();
        const auto limit         = static_cast<std::uint64_t>(filter.limit);
        const auto offset        = filter.offset;
        const std::string source = " FROM " + std::string{table} + " l LEFT JOIN users u ON u.id = l.actor_user_id WHERE ";
        const std::string predicate{"(? = '' OR l.created_at >= ?) AND (? = '' OR l.created_at <= ?) AND "
                                    "(? = '' OR l.event_name = ?) AND (? = '' OR l.result = ?) AND "
                                    "(? = '' OR l.actor_user_id = ?) AND "
                                    "(? = '' OR u.display_name LIKE ? OR u.login_identifier LIKE ?) AND "
                                    "(? = '' OR l.event_name LIKE ? OR JSON_CONTAINS(?, JSON_QUOTE(l.event_name)))"};

        const auto totals = connection_.query_s<std::tuple<std::uint64_t>>(
            "SELECT COUNT(*)" + source + predicate, from, from, to, to, event, event, result, result, actor, actor, username,
            pattern, pattern, eventSearch, eventPattern, eventNames);
        if (connection_.has_error() || totals.size() != 1)
            return queryFailed<AUDIT::AuditPage>();

        AUDIT::AuditPage page{.items = {}, .total = std::get<0>(totals.front())};
        if (page.total <= offset)
            return Result<AUDIT::AuditPage>::Success(std::move(page));

        auto rows = connection_.query_s<AuditPageRecord>(
            "SELECT l.id, l.actor_user_id, l.target_user_id, l.event_name, l.result, l.request_id, "
            "CASE WHEN OCTET_LENGTH(l.details_json) <= 1048576 THEN CAST(l.details_json AS CHAR) ELSE '{}' END, "
            "l.created_at, COALESCE(u.login_identifier, ''), " +
                std::string{"'"} + std::string{table} + "'" + source + predicate +
                " ORDER BY l.created_at DESC, l.id ASC LIMIT ? OFFSET ?",
            from, from, to, to, event, event, result, result, actor, actor, username, pattern, pattern, eventSearch,
            eventPattern, eventNames, limit, offset);
        if (connection_.has_error())
            return queryFailed<AUDIT::AuditPage>();
        page.items.reserve(rows.size());
        for (auto &row : rows)
        {
            page.items.push_back({.id             = std::move(row.id),
                                  .actor_user_id  = std::move(row.actor_user_id),
                                  .target_user_id = std::move(row.target_user_id),
                                  .event_name     = std::move(row.event_name),
                                  .result         = std::move(row.result),
                                  .request_id     = std::move(row.request_id),
                                  .details_json   = std::move(row.details_json),
                                  .created_at     = std::move(row.created_at),
                                  .actor_login_identifier = std::move(row.actor_login_identifier)});
        }
        return Result<AUDIT::AuditPage>::Success(std::move(page));
    } catch (...)
    {
        return queryFailed<AUDIT::AuditPage>();
    }
}
} // namespace SZX::ADMIN::INFRA::DETAIL
