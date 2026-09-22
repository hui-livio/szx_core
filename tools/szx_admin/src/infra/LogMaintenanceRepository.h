#pragma once

#include "../maintenance/LogMaintenanceRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;

class MySqlLogMaintenanceRepository final : public MAINTENANCE::LogMaintenanceRepository
{
public:
    explicit MySqlLogMaintenanceRepository(OrmConnection &connection) :
        connection_(connection)
    {
    }

    auto purgeAuditBatch(std::string cutoff, std::uint32_t batchSize) -> SZX::COMMON::Result<MAINTENANCE::LogPurgeBatch> override;
    auto purgeBehaviorBatch(std::string cutoff, std::uint32_t batchSize)
        -> SZX::COMMON::Result<MAINTENANCE::LogPurgeBatch> override;

private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
