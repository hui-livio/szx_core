#pragma once

#include "LogMaintenanceRepository.h"

#include "SzxAdmin/Infra/Database.h"

#include <functional>

namespace SZX::ADMIN::MAINTENANCE
{
struct LogPurgeSummary final
{
    std::uint64_t auditDeleted{};
    std::uint64_t behaviorDeleted{};
};

class LogMaintenanceService final
{
public:
    using ConnectionExecutor = std::function<SZX::COMMON::Result<void>(const INFRA::DatabasePool::ConnectionCallback &)>;

    LogMaintenanceService(INFRA::DatabasePool &database, INFRA::LogStorageConfig config);
    LogMaintenanceService(ConnectionExecutor execute, INFRA::LogStorageConfig config);

    [[nodiscard]] auto purge() -> SZX::COMMON::Result<LogPurgeSummary>;

private:
    ConnectionExecutor execute_;
    INFRA::LogStorageConfig config_;
};
} // namespace SZX::ADMIN::MAINTENANCE
