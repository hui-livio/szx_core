#pragma once

#include "AuthorizationService.h"
#include "PermissionRepository.h"
#include "../user/ActorContext.h"

#include "SzxAdmin/Infra/Database.h"

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::RBAC
{
struct NewPermissionInput final
{
    std::string code;
    std::string name;
    std::string description;
    std::string module;
};

class PermissionService final
{
public:
    PermissionService(INFRA::DatabasePool &database, AUTH::LoginService &auth);

    [[nodiscard]] auto list(const USER::ActorContext &context, bool includeArchived = false)
        -> SZX::COMMON::Result<std::vector<PermissionRow>>;
    [[nodiscard]] auto get(const USER::ActorContext &context, std::string_view id)
        -> SZX::COMMON::Result<PermissionRow>;
    [[nodiscard]] auto create(const USER::ActorContext &context, const NewPermissionInput &input)
        -> SZX::COMMON::Result<PermissionRow>;
    [[nodiscard]] auto updateProfile(const USER::ActorContext &context, std::string_view id, std::string_view name,
                                     std::string_view description, std::uint32_t version)
        -> SZX::COMMON::Result<PermissionRow>;
    [[nodiscard]] auto changeStatus(const USER::ActorContext &context, std::string_view id, std::string_view status,
                                    std::uint32_t version) -> SZX::COMMON::Result<PermissionRow>;

private:
    INFRA::DatabasePool &database_;
    AuthorizationService authorization_;
};
} // namespace SZX::ADMIN::RBAC
