#include <gtest/gtest.h>

#include "../../tools/szx_admin/src/auth/LoginService.h"
#include "../../tools/szx_admin/src/rbac/AuthorizationService.h"
#include "../../tools/szx_admin/include/SzxAdmin/Policy/Policy.h"
#include "../../tools/szx_admin/src/rbac/PermissionCodes.h"

namespace
{
using SZX::ADMIN::POLICY::Capability;
using SZX::ADMIN::POLICY::allows;
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
