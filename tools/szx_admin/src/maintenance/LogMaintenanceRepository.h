#pragma once

#include "../audit/AuditRepository.h"

#include "Result.hpp"

#include <cstdint>
#include <string>

namespace SZX::ADMIN::MAINTENANCE
{
struct LogPurgeBatch final
{
    std::uint64_t deleted{};
};

class LogMaintenanceRepository
{
public:
    virtual ~LogMaintenanceRepository() = default;

    [[nodiscard]] virtual auto purgeAuditBatch(std::string cutoff, std::uint32_t batchSize)
        -> SZX::COMMON::Result<LogPurgeBatch> = 0;
    [[nodiscard]] virtual auto purgeBehaviorBatch(std::string cutoff, std::uint32_t batchSize)
        -> SZX::COMMON::Result<LogPurgeBatch> = 0;
};
} // namespace SZX::ADMIN::MAINTENANCE
