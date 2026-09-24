#include <gtest/gtest.h>

#include "../../tools/szx_admin/src/auth/LoginService.h"
#include "../../tools/szx_admin/src/rbac/AuthorizationService.h"
#include "../../tools/szx_admin/src/rbac/RoleRepository.h"
#include "../../tools/szx_admin/include/SzxAdmin/Policy/Policy.h"
#include "../../tools/szx_admin/src/rbac/PermissionCodes.h"

namespace
{
using SZX::ADMIN::POLICY::Capability;
using SZX::ADMIN::POLICY::allows;

class FakeRoleRepository final : public SZX::ADMIN::RBAC::RoleRepository
{
public:
    auto findById(std::string_view, bool) -> SZX::COMMON::Result<std::optional<SZX::ADMIN::RBAC::RoleRow>> override
    {
        return SZX::COMMON::Result<std::optional<SZX::ADMIN::RBAC::RoleRow>>::Success(std::nullopt);
    }

    auto findByCode(std::string_view code, bool) -> SZX::COMMON::Result<std::optional<SZX::ADMIN::RBAC::RoleRow>> override
    {
        if (code != "custom_viewer" && code != "admin")
            return SZX::COMMON::Result<std::optional<SZX::ADMIN::RBAC::RoleRow>>::Success(std::nullopt);
        return SZX::COMMON::Result<std::optional<SZX::ADMIN::RBAC::RoleRow>>::Success(
            SZX::ADMIN::RBAC::RoleRow{.code = std::string{code}, .status = "active"});
    }

    auto hasPermission(std::string_view role, std::string_view permission) -> SZX::COMMON::Result<bool> override
    {
        return SZX::COMMON::Result<bool>::Success(role == "custom_viewer" && permission == "user.read");
    }

    auto canManageRole(std::string_view manager, std::string_view target) -> SZX::COMMON::Result<bool> override
    {
        return SZX::COMMON::Result<bool>::Success(manager == "admin" && target == "custom_viewer");
    }

    auto manageableRoles(std::string_view manager) -> SZX::COMMON::Result<std::vector<std::string>> override
    {
        if (manager != "admin")
            return SZX::COMMON::Result<std::vector<std::string>>::Success({});
        return SZX::COMMON::Result<std::vector<std::string>>::Success({"custom_viewer"});
    }
};
}

TEST(AdminAuthorizationPolicyTest, ExposesSingleRoleCatalogForApplicationFilters)
{
    EXPECT_TRUE(SZX::ADMIN::POLICY::validRole("operator"));
    EXPECT_TRUE(SZX::ADMIN::POLICY::validRole("engineer"));
    EXPECT_TRUE(SZX::ADMIN::POLICY::validRole("admin"));
    EXPECT_TRUE(SZX::ADMIN::POLICY::validRole("super_admin"));
    EXPECT_FALSE(SZX::ADMIN::POLICY::validRole("custom_role"));
}

TEST(AdminAuthorizationPolicyTest, StablePermissionCodesRemainMappedToExistingPolicy)
{
    using namespace SZX::ADMIN::RBAC::PERMISSIONS;
    EXPECT_TRUE(allows("admin", Capability::UserRead));
    EXPECT_TRUE(allows("super_admin", Capability::UserRoleAssign));
    EXPECT_FALSE(allows("admin", Capability::UserRoleAssign));
    EXPECT_EQ(UserRead, "user.read");
    EXPECT_EQ(UserRoleAssign, "user.role.assign");
}

TEST(AdminAuthorizationPolicyTest, AuthorizationServiceOwnsManageableRoleProjection)
{
    SZX::ADMIN::INFRA::DatabasePool database(SZX::ADMIN::INFRA::DatabaseConfig{});
    SZX::ADMIN::AUTH::LoginService login(database, {});
    SZX::ADMIN::RBAC::AuthorizationService authorization(login);

    const auto manageable = authorization.manageableRoles("admin");
    ASSERT_EQ(manageable.size(), 2U);
    EXPECT_EQ(manageable[0], "operator");
    EXPECT_EQ(manageable[1], "engineer");
    EXPECT_TRUE(authorization.allows("admin", SZX::ADMIN::RBAC::PERMISSIONS::UserRead));
    EXPECT_FALSE(authorization.allows("admin", "role.unknown"));
}

TEST(AdminAuthorizationPolicyTest, DynamicRepositoryCanExposeCustomRolePermissions)
{
    SZX::ADMIN::INFRA::DatabasePool database(SZX::ADMIN::INFRA::DatabaseConfig{});
    SZX::ADMIN::AUTH::LoginService login(database, {});
    SZX::ADMIN::RBAC::AuthorizationService authorization(login);
    FakeRoleRepository roles;

    const auto allowed = authorization.allows(roles, "custom_viewer", "user.read");
    ASSERT_TRUE(allowed.ok);
    EXPECT_TRUE(allowed.value);
    const auto manageable = authorization.manageableRoles(roles, "admin");
    ASSERT_TRUE(manageable.ok);
    ASSERT_EQ(manageable.value.size(), 1U);
    EXPECT_EQ(manageable.value.front(), "custom_viewer");
}
