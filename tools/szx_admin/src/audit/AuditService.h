#pragma once

#include "../user/ActorContext.h"
#include "../rbac/AuthorizationService.h"
#include "AuditRepository.h"

#include "SzxAdmin/Infra/Database.h"

#include <string>

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::AUDIT
{
struct AuditQuery final
{
    std::string from;
    std::string to;
    std::string eventName;
    std::string result;
    std::string actorUserId;
    std::uint32_t page{1};
    std::uint32_t pageSize{20};
    std::string actorSearch;
    std::string eventSearch;
};

class AuditService final
{
public:
    AuditService(INFRA::DatabasePool &database, AUTH::LoginService &auth);

    [[nodiscard]] auto list(const USER::ActorContext &actor, const AuditQuery &query) -> SZX::COMMON::Result<AuditPage>;

private:
    auto query(const USER::ActorContext &actor, const AuditQuery &query) -> SZX::COMMON::Result<AuditPage>;

    INFRA::DatabasePool &database_;
    RBAC::AuthorizationService authorization_;
};
} // namespace SZX::ADMIN::AUDIT
