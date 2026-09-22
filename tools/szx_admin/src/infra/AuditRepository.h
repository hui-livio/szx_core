#pragma once

#include "../audit/AuditRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;

class MySqlAuditRepository final : public AUDIT::AuditRepository
{
public:
    explicit MySqlAuditRepository(OrmConnection &connection) :
        connection_(connection)
    {
    }

    auto append(const AUDIT::AuditRecord &record) -> SZX::COMMON::Result<void> override;
    auto query(const AUDIT::AuditFilter &filter) -> SZX::COMMON::Result<AUDIT::AuditPage> override;

private:
    auto queryTable(const AUDIT::AuditFilter &filter, std::string_view table) -> SZX::COMMON::Result<AUDIT::AuditPage>;

    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
