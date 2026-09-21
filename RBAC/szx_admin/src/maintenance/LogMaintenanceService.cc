#include "LogMaintenanceService.h"

#include "Defer.hpp"
#include "JsonHelper.hpp"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"
#include "UUID.hpp"

#include <iomanip>
#include <sstream>

namespace SZX::ADMIN::MAINTENANCE
{
namespace
{
using INFRA::ErrorCode;
using SZX::COMMON::Result;

template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto timestamp(std::int64_t milliseconds) -> std::string
{
    std::ostringstream stream;
    stream << SZX::COMMON::TimeHelper::TimestampToString(milliseconds) << '.' << std::setw(3) << std::setfill('0')
           << milliseconds % 1000 << "000";
    return stream.str();
}

auto cutoff(std::uint32_t days) -> std::string
{
    constexpr std::int64_t millisecondsPerDay = 24LL * 60 * 60 * 1000;
    return timestamp(SZX::COMMON::TimeHelper::NowTimestamp() - static_cast<std::int64_t>(days) * millisecondsPerDay);
}

auto commitTransaction(INFRA::DatabaseConnection &connection) -> Result<void>
{
    const auto committed = connection.commitWithOutcome();
    if (committed.ok && committed.value == INFRA::CommitOutcome::Committed)
        return Result<void>::Success();
    if (committed.ok && committed.value == INFRA::CommitOutcome::NotCommitted)
        (void)connection.rollback();
    else
        connection.invalidate();
    return failed<void>(ErrorCode::DatabaseTransactionFailed);
}

auto purgeAudit(std::string eventName, std::string requestId, std::string cutoffValue, std::uint64_t primary,
                std::uint64_t legacy) -> AUDIT::AuditRecord
{
    const auto details =
        SZX::COMMON::Json{{"cutoff", std::move(cutoffValue)}, {"primary_deleted", primary}, {"legacy_deleted", legacy}};
    return {.id           = SZX::COMMON::Uuid::v4().toString(),
            .event_name   = std::move(eventName),
            .result       = "success",
            .request_id   = std::move(requestId),
            .ip           = "127.0.0.1",
            .user_agent   = "szx_admin_maintenance",
            .details_json = details.dump()};
}
} // namespace

LogMaintenanceService::LogMaintenanceService(INFRA::DatabasePool &database, INFRA::LogStorageConfig config) :
    LogMaintenanceService([&database](const INFRA::DatabasePool::ConnectionCallback &callback)
                          { return database.withConnection(callback); }, std::move(config))
{
}

LogMaintenanceService::LogMaintenanceService(ConnectionExecutor execute, INFRA::LogStorageConfig config) :
    execute_(std::move(execute)),
    config_(std::move(config))
{
}

auto LogMaintenanceService::inspect() -> Result<LogMigrationStatus>
{
    LogMigrationStatus status;
    const auto operation = execute_(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            auto *repository = connection.logMaintenanceRepository();
            if (!repository)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto inspected = repository->inspect();
            if (!inspected.ok)
                return Result<void>::Failure(inspected.error, {});
            status = inspected.value;
            return Result<void>::Success();
        });
    return operation.ok ? Result<LogMigrationStatus>::Success(status) : Result<LogMigrationStatus>::Failure(operation.error, {});
}

auto LogMaintenanceService::backfill() -> Result<LogBackfillSummary>
{
    LogBackfillSummary summary;
    const auto operation = execute_(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            auto *repository = connection.logMaintenanceRepository();
            if (!repository)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto acquired = connection.acquireMigrationLock("szx_log_split_maintenance", 0);
            if (!acquired.ok || !acquired.value)
                return failed<void>(ErrorCode::ServiceUnavailable);
            defer
            {
                (void)connection.releaseMigrationLock("szx_log_split_maintenance");
            };

            const auto before = repository->inspect();
            if (!before.ok)
                return Result<void>::Failure(before.error, {});
            summary.before = before.value;
            if (before.value.conflictingAuditRows != 0 || before.value.conflictingBehaviorRows != 0)
                return failed<void>(ErrorCode::MigrationConflict);
            for (;;)
            {
                const auto begun = connection.begin();
                if (!begun.ok)
                    return begun;
                const auto batch = repository->backfillBatch(config_.cleanupBatchSize);
                if (!batch.ok)
                {
                    (void)connection.rollback();
                    return Result<void>::Failure(batch.error, {});
                }
                if (batch.value.auditInserted == 0 && batch.value.behaviorInserted == 0)
                {
                    (void)connection.rollback();
                    break;
                }
                const auto committed = commitTransaction(connection);
                if (!committed.ok)
                    return committed;
                summary.auditInserted    += batch.value.auditInserted;
                summary.behaviorInserted += batch.value.behaviorInserted;
            }
            const auto after = repository->inspect();
            if (!after.ok)
                return Result<void>::Failure(after.error, {});
            summary.after = after.value;
            if (after.value.missingAuditRows != 0 || after.value.missingBehaviorRows != 0 ||
                after.value.conflictingAuditRows != 0 || after.value.conflictingBehaviorRows != 0)
                return failed<void>(ErrorCode::MigrationConflict);
            return Result<void>::Success();
        });
    return operation.ok ? Result<LogBackfillSummary>::Success(summary) : Result<LogBackfillSummary>::Failure(operation.error, {});
}

auto LogMaintenanceService::purge() -> Result<LogPurgeSummary>
{
    LogPurgeSummary summary;
    const auto auditCutoff    = cutoff(config_.auditRetentionDays);
    const auto behaviorCutoff = cutoff(config_.behaviorRetentionDays);
    const auto operation      = execute_(
        [&](INFRA::DatabaseConnection &connection) -> Result<void>
        {
            auto *repository = connection.logMaintenanceRepository();
            auto *audit      = connection.auditRepository();
            if (!repository || !audit)
                return failed<void>(ErrorCode::DatabaseQueryFailed);
            const auto acquired = connection.acquireMigrationLock("szx_log_split_maintenance", 0);
            if (!acquired.ok || !acquired.value)
                return failed<void>(ErrorCode::ServiceUnavailable);
            defer
            {
                (void)connection.releaseMigrationLock("szx_log_split_maintenance");
            };

            const auto purgeType = [&](bool behavior) -> Result<void>
            {
                for (;;)
                {
                    const auto begun = connection.begin();
                    if (!begun.ok)
                        return begun;
                    const auto batch = behavior ? repository->purgeBehaviorBatch(behaviorCutoff, config_.cleanupBatchSize) :
                                                       repository->purgeAuditBatch(auditCutoff, config_.cleanupBatchSize);
                    if (!batch.ok)
                    {
                        (void)connection.rollback();
                        return Result<void>::Failure(batch.error, {});
                    }
                    if (batch.value.primaryDeleted == 0)
                    {
                        (void)connection.rollback();
                        return Result<void>::Success();
                    }
                    const auto event =
                        purgeAudit(behavior ? "user.behavior_events.purged" : "user.audit_logs.purged",
                                   "maintenance-" + SZX::COMMON::Uuid::v4().toString(), behavior ? behaviorCutoff : auditCutoff,
                                   batch.value.primaryDeleted, batch.value.legacyDeleted);
                    const auto recorded = audit->append(event);
                    if (!recorded.ok)
                    {
                        (void)connection.rollback();
                        return recorded;
                    }
                    const auto committed = commitTransaction(connection);
                    if (!committed.ok)
                        return committed;
                    if (behavior)
                    {
                        summary.behaviorDeleted       += batch.value.primaryDeleted;
                        summary.behaviorLegacyDeleted += batch.value.legacyDeleted;
                    }
                    else
                    {
                        summary.auditDeleted       += batch.value.primaryDeleted;
                        summary.auditLegacyDeleted += batch.value.legacyDeleted;
                    }
                }
            };
            const auto behavior = purgeType(true);
            return behavior.ok ? purgeType(false) : behavior;
        });
    return operation.ok ? Result<LogPurgeSummary>::Success(summary) : Result<LogPurgeSummary>::Failure(operation.error, {});
}
} // namespace SZX::ADMIN::MAINTENANCE
