#pragma once

#include "../telemetry/BehaviorEventRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;

class MySqlBehaviorEventRepository final : public TELEMETRY::BehaviorEventRepository
{
public:
    explicit MySqlBehaviorEventRepository(OrmConnection &connection) :
        connection_(connection)
    {
    }

    auto appendBatch(std::span<const TELEMETRY::BehaviorEventRecord> records)
        -> SZX::COMMON::Result<TELEMETRY::BatchWriteOutcome> override;
    auto probe() -> SZX::COMMON::Result<void> override;
    auto query(const TELEMETRY::BehaviorEventFilter &filter) -> SZX::COMMON::Result<TELEMETRY::BehaviorEventPage> override;

private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
