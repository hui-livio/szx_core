#pragma once

#include "../audit/AuditRepository.h"

#include "Result.hpp"

#include <cstdint>
#include <string>

namespace SZX::ADMIN::MAINTENANCE
{
struct LogMigrationStatus final
{
    std::uint64_t legacyRows{};
    std::uint64_t eligibleAuditRows{};
    std::uint64_t eligibleBehaviorRows{};
    std::uint64_t legacyUnknownRows{};
    std::uint64_t auditRows{};
    std::uint64_t behaviorRows{};
    std::uint64_t missingAuditRows{};
    std::uint64_t missingBehaviorRows{};
    std::uint64_t conflictingAuditRows{};
    std::uint64_t conflictingBehaviorRows{};
};

struct LogBackfillBatch final
{
    std::uint64_t auditInserted{};
    std::uint64_t behaviorInserted{};
};

struct LogPurgeBatch final
{
    std::uint64_t primaryDeleted{};
    std::uint64_t legacyDeleted{};
};

class LogMaintenanceRepository
{
public:
    virtual ~LogMaintenanceRepository() = default;

    [[nodiscard]] virtual auto inspect() -> SZX::COMMON::Result<LogMigrationStatus>                            = 0;
    [[nodiscard]] virtual auto backfillBatch(std::uint32_t batchSize) -> SZX::COMMON::Result<LogBackfillBatch> = 0;
    [[nodiscard]] virtual auto purgeAuditBatch(std::string cutoff, std::uint32_t batchSize)
        -> SZX::COMMON::Result<LogPurgeBatch> = 0;
    [[nodiscard]] virtual auto purgeBehaviorBatch(std::string cutoff, std::uint32_t batchSize)
        -> SZX::COMMON::Result<LogPurgeBatch> = 0;
};
} // namespace SZX::ADMIN::MAINTENANCE
