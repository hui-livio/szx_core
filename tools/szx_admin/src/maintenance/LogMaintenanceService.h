#pragma once

#include "LogMaintenanceRepository.h"

#include "SzxAdmin/Infra/Database.h"

#include <functional>

namespace SZX::ADMIN::MAINTENANCE
{
struct LogBackfillSummary final
{
    LogMigrationStatus before;
    LogMigrationStatus after;
    std::uint64_t auditInserted{};
    std::uint64_t behaviorInserted{};
};

struct LogPurgeSummary final
{
    std::uint64_t auditDeleted{};
    std::uint64_t auditLegacyDeleted{};
    std::uint64_t behaviorDeleted{};
    std::uint64_t behaviorLegacyDeleted{};
};

class LogMaintenanceService final
{
public:
    using ConnectionExecutor = std::function<SZX::COMMON::Result<void>(const INFRA::DatabasePool::ConnectionCallback &)>;

    LogMaintenanceService(INFRA::DatabasePool &database, INFRA::LogStorageConfig config);
    LogMaintenanceService(ConnectionExecutor execute, INFRA::LogStorageConfig config);

    [[nodiscard]] auto inspect() -> SZX::COMMON::Result<LogMigrationStatus>;
    [[nodiscard]] auto backfill() -> SZX::COMMON::Result<LogBackfillSummary>;
    [[nodiscard]] auto purge() -> SZX::COMMON::Result<LogPurgeSummary>;

private:
    ConnectionExecutor execute_;
    INFRA::LogStorageConfig config_;
};
} // namespace SZX::ADMIN::MAINTENANCE
