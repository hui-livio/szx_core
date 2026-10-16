#pragma once

#include "AuthorizationService.h"
#include "RoleRepository.h"
#include "../user/ActorContext.h"

#include "SzxAdmin/Infra/Database.h"

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::RBAC
{
struct NewRoleInput final
{
    std::string code;
    std::string name;
    std::string description;
};

class RoleService final
{
public:
    RoleService(INFRA::DatabasePool &database, AUTH::LoginService &auth);

    [[nodiscard]] auto list(const USER::ActorContext &context, bool includeArchived = false)
        -> SZX::COMMON::Result<std::vector<RoleRow>>;
    [[nodiscard]] auto get(const USER::ActorContext &context, std::string_view id)
        -> SZX::COMMON::Result<RoleRow>;
    [[nodiscard]] auto create(const USER::ActorContext &context, const NewRoleInput &input)
        -> SZX::COMMON::Result<RoleRow>;
    [[nodiscard]] auto updateProfile(const USER::ActorContext &context, std::string_view id, std::string_view name,
                                     std::string_view description, std::uint32_t version)
        -> SZX::COMMON::Result<RoleRow>;
    [[nodiscard]] auto changeStatus(const USER::ActorContext &context, std::string_view id, std::string_view status,
                                    std::uint32_t version) -> SZX::COMMON::Result<RoleRow>;
    [[nodiscard]] auto permissionIds(const USER::ActorContext &context, std::string_view id)
        -> SZX::COMMON::Result<std::vector<std::string>>;
    [[nodiscard]] auto manageScopeIds(const USER::ActorContext &context, std::string_view id)
        -> SZX::COMMON::Result<std::vector<std::string>>;
    [[nodiscard]] auto replacePermissionIds(const USER::ActorContext &context, std::string_view id,
                                             const std::vector<std::string> &permissionIds, std::uint32_t version)
        -> SZX::COMMON::Result<RoleRow>;
    [[nodiscard]] auto replaceManageScopeIds(const USER::ActorContext &context, std::string_view id,
                                             const std::vector<std::string> &targetRoleIds, std::uint32_t version)
        -> SZX::COMMON::Result<RoleRow>;

private:
    INFRA::DatabasePool &database_;
    AuthorizationService authorization_;
};
} // namespace SZX::ADMIN::RBAC
