#include "LogMaintenanceRepository.h"

#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <cctype>
#include <tuple>

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
using SZX::COMMON::Result;

template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto scalar(OrmConnection &connection, const std::string &sql) -> Result<std::uint64_t>
{
    const auto rows = connection.query_s<std::tuple<std::uint64_t>>(sql);
    if (connection.has_error() || rows.size() != 1)
        return failed<std::uint64_t>(ErrorCode::DatabaseQueryFailed);
    return Result<std::uint64_t>::Success(std::get<0>(rows.front()));
}

auto validCutoff(std::string_view value) -> bool
{
    if (value.size() != 26)
        return false;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const bool separator = (index == 4 || index == 7) ? value[index] == '-' :
            (index == 10)                                  ? value[index] == ' ' :
            (index == 13 || index == 16)                   ? value[index] == ':' :
            (index == 19)                                  ? value[index] == '.' :
                                                             std::isdigit(static_cast<unsigned char>(value[index])) != 0;
        if (!separator)
            return false;
    }
    return true;
}

auto purgeBatch(OrmConnection &connection, std::string_view table, std::string_view cutoff, std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogPurgeBatch>
{
    if ((table != "security_audit_logs" && table != "client_behavior_events") || !validCutoff(cutoff) || batchSize == 0)
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::ValidationError);
    if (!connection.execute(
            "CREATE TEMPORARY TABLE IF NOT EXISTS szx_log_purge_ids (id CHAR(36) NOT NULL PRIMARY KEY) ENGINE=MEMORY") ||
        !connection.execute("DELETE FROM szx_log_purge_ids") ||
        !connection.execute("INSERT INTO szx_log_purge_ids(id) SELECT id FROM " + std::string{table} +
                            " WHERE created_at < '" + std::string{cutoff} + "' ORDER BY created_at, id LIMIT " +
                            std::to_string(batchSize)))
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseQueryFailed);

    const auto selected = connection.affectedRows();
    if (selected == 0)
        return Result<MAINTENANCE::LogPurgeBatch>::Success({});
    if (!connection.execute(
            "DELETE l FROM user_logs l INNER JOIN szx_log_purge_ids p ON p.id = l.id"))
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseQueryFailed);
    const auto legacyDeleted = connection.affectedRows();
    if (!connection.execute("DELETE t FROM " + std::string{table} +
                            " t INNER JOIN szx_log_purge_ids p ON p.id = t.id"))
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseQueryFailed);
    const auto primaryDeleted = connection.affectedRows();
    if (primaryDeleted != selected)
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseTransactionFailed);
    return Result<MAINTENANCE::LogPurgeBatch>::Success(
        {.primaryDeleted = primaryDeleted, .legacyDeleted = legacyDeleted});
}
} // namespace

auto MySqlLogMaintenanceRepository::inspect() -> Result<MAINTENANCE::LogMigrationStatus>
{
    try
    {
        MAINTENANCE::LogMigrationStatus status;
        const auto load = [&](std::uint64_t &target, const std::string &sql) -> bool
        {
            const auto value = scalar(connection_, sql);
            if (!value.ok)
                return false;
            target = value.value;
            return true;
        };
        const std::string auditClass = "(l.event_name LIKE 'auth.%' OR l.event_name LIKE 'user.%')";
        const std::string behaviorClass = "(NOT " + auditClass +
            " AND l.actor_user_id IS NOT NULL AND l.actor_user_id = l.target_user_id "
            "AND l.result IN ('success','failure'))";
        if (!load(status.legacyRows, "SELECT COUNT(*) FROM user_logs") ||
            !load(status.eligibleAuditRows, "SELECT COUNT(*) FROM user_logs l WHERE " + auditClass) ||
            !load(status.eligibleBehaviorRows, "SELECT COUNT(*) FROM user_logs l WHERE " + behaviorClass) ||
            !load(status.legacyUnknownRows,
                  "SELECT COUNT(*) FROM user_logs l WHERE NOT " + auditClass + " AND NOT " + behaviorClass) ||
            !load(status.auditRows, "SELECT COUNT(*) FROM security_audit_logs") ||
            !load(status.behaviorRows, "SELECT COUNT(*) FROM client_behavior_events") ||
            !load(status.missingAuditRows,
                  "SELECT COUNT(*) FROM user_logs l WHERE " + auditClass +
                      " AND NOT EXISTS (SELECT 1 FROM security_audit_logs a WHERE a.id=l.id)") ||
            !load(status.missingBehaviorRows,
                  "SELECT COUNT(*) FROM user_logs l WHERE " + behaviorClass +
                      " AND NOT EXISTS (SELECT 1 FROM client_behavior_events b WHERE b.id=l.id)") ||
            !load(status.conflictingAuditRows,
                  "SELECT COUNT(*) FROM user_logs l INNER JOIN security_audit_logs a ON a.id=l.id WHERE " + auditClass +
                      " AND (NOT (a.actor_user_id <=> l.actor_user_id) OR NOT (a.target_user_id <=> l.target_user_id) "
                      "OR a.event_name<>l.event_name OR a.result<>l.result OR a.request_id<>l.request_id "
                      "OR NOT (a.ip <=> l.ip) OR NOT (a.user_agent <=> l.user_agent) "
                      "OR NOT (a.details_json <=> l.details_json) OR a.created_at<>l.created_at)") ||
            !load(status.conflictingBehaviorRows,
                  "SELECT COUNT(*) FROM user_logs l INNER JOIN client_behavior_events b ON b.id=l.id WHERE " +
                      behaviorClass +
                      " AND (b.user_id<>l.actor_user_id OR b.event_name<>l.event_name OR b.result<>l.result "
                      "OR b.request_id<>l.request_id OR NOT (b.ip <=> l.ip) OR NOT (b.user_agent <=> l.user_agent) "
                      "OR NOT (b.details_json <=> l.details_json) OR b.created_at<>l.created_at)"))
            return failed<MAINTENANCE::LogMigrationStatus>(ErrorCode::DatabaseQueryFailed);
        return Result<MAINTENANCE::LogMigrationStatus>::Success(status);
    } catch (...)
    {
        return failed<MAINTENANCE::LogMigrationStatus>(ErrorCode::DatabaseQueryFailed);
    }
}

auto MySqlLogMaintenanceRepository::backfillBatch(std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogBackfillBatch>
{
    try
    {
        if (batchSize == 0)
            return failed<MAINTENANCE::LogBackfillBatch>(ErrorCode::ValidationError);
        const auto status = inspect();
        if (!status.ok)
            return Result<MAINTENANCE::LogBackfillBatch>::Failure(status.error, {});
        if (status.value.conflictingAuditRows != 0 || status.value.conflictingBehaviorRows != 0)
            return failed<MAINTENANCE::LogBackfillBatch>(ErrorCode::MigrationConflict);

        const auto limit = std::to_string(batchSize);
        if (!connection_.execute(
                "INSERT INTO security_audit_logs "
                "(id,actor_user_id,target_user_id,event_name,result,request_id,ip,user_agent,details_json,created_at) "
                "SELECT l.id,l.actor_user_id,l.target_user_id,l.event_name,l.result,l.request_id,l.ip,l.user_agent,"
                "l.details_json,l.created_at FROM user_logs l WHERE "
                "(l.event_name LIKE 'auth.%' OR l.event_name LIKE 'user.%') "
                "AND NOT EXISTS (SELECT 1 FROM security_audit_logs a WHERE a.id=l.id) "
                "ORDER BY l.created_at,l.id LIMIT " +
                limit))
            return failed<MAINTENANCE::LogBackfillBatch>(ErrorCode::DatabaseQueryFailed);
        const auto auditInserted = connection_.affectedRows();
        if (!connection_.execute(
                "INSERT INTO client_behavior_events "
                "(id,user_id,event_name,result,request_id,ip,user_agent,details_json,created_at,persisted_at) "
                "SELECT l.id,l.actor_user_id,l.event_name,l.result,l.request_id,l.ip,l.user_agent,l.details_json,"
                "l.created_at,l.created_at FROM user_logs l WHERE "
                "NOT (l.event_name LIKE 'auth.%' OR l.event_name LIKE 'user.%') "
                "AND l.actor_user_id IS NOT NULL AND l.actor_user_id=l.target_user_id "
                "AND l.result IN ('success','failure') "
                "AND NOT EXISTS (SELECT 1 FROM client_behavior_events b WHERE b.id=l.id) "
                "ORDER BY l.created_at,l.id LIMIT " +
                limit))
            return failed<MAINTENANCE::LogBackfillBatch>(ErrorCode::DatabaseQueryFailed);
        return Result<MAINTENANCE::LogBackfillBatch>::Success(
            {.auditInserted = auditInserted, .behaviorInserted = connection_.affectedRows()});
    } catch (...)
    {
        return failed<MAINTENANCE::LogBackfillBatch>(ErrorCode::DatabaseQueryFailed);
    }
}

auto MySqlLogMaintenanceRepository::purgeAuditBatch(std::string cutoff, std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogPurgeBatch>
{
    return purgeBatch(connection_, "security_audit_logs", cutoff, batchSize);
}

auto MySqlLogMaintenanceRepository::purgeBehaviorBatch(std::string cutoff, std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogPurgeBatch>
{
    return purgeBatch(connection_, "client_behavior_events", cutoff, batchSize);
}
} // namespace SZX::ADMIN::INFRA::DETAIL
