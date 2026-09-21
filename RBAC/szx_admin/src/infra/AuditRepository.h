#pragma once

#include "../audit/AuditRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;

class MySqlAuditRepository final : public AUDIT::AuditRepository
{
public:
    explicit MySqlAuditRepository(OrmConnection &connection, bool dualWriteLegacy, bool readLegacy) :
        connection_(connection),
        dualWriteLegacy_(dualWriteLegacy),
        readLegacy_(readLegacy)
    {
    }

    auto append(const AUDIT::AuditRecord &record) -> SZX::COMMON::Result<void> override;
    auto query(const AUDIT::AuditFilter &filter) -> SZX::COMMON::Result<AUDIT::AuditPage> override;
    auto queryLegacy(const AUDIT::AuditFilter &filter) -> SZX::COMMON::Result<AUDIT::AuditPage> override;
    auto queryCompatibility(const AUDIT::AuditFilter &filter) -> SZX::COMMON::Result<AUDIT::AuditPage> override;

private:
    auto queryTable(const AUDIT::AuditFilter &filter, std::string_view table)
        -> SZX::COMMON::Result<AUDIT::AuditPage>;

    OrmConnection &connection_;
    bool dualWriteLegacy_{};
    bool readLegacy_{};
};
} // namespace SZX::ADMIN::INFRA::DETAIL
