#include <gtest/gtest.h>

#include "../../tools/szx_admin/src/auth/LoginService.h"
#include "../../tools/szx_admin/src/rbac/RoleDto.h"
#include "../../tools/szx_admin/src/rbac/RoleService.h"
#include "JsonHelper.hpp"
#include "SzxAdmin/Infra/RestApi.h"

using namespace SZX::ADMIN;

TEST(RoleManagementTest, RejectsInvalidRoleInputBeforeStorage)
{
    INFRA::DatabasePool database{INFRA::DatabaseConfig{}};
    AUTH::LoginService login{database, {}};
    RBAC::RoleService service{database, login};
    for (const auto &code : {"", "a", "UPPER", "bad.code", "1viewer", "bad code"})
    {
        SCOPED_TRACE(code);
        const auto result = service.create({}, {code, "Viewer", ""});
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error, static_cast<int>(INFRA::ErrorCode::ValidationError));
    }
    for (const auto code : {INFRA::ErrorCode::RoleCodeConflict, INFRA::ErrorCode::RoleInUse, INFRA::ErrorCode::ProtectedRole})
    {
        EXPECT_EQ(INFRA::errorCodeHttpStatus(code), 409);
        const auto response = INFRA::RestApi::error(code, "role-error");
        EXPECT_EQ(response.statusCode, 409);
        const auto parsed = SZX::COMMON::JsonHelper::Parse(response.body);
        EXPECT_TRUE(parsed.has_value());
        if (parsed)
            EXPECT_EQ(parsed->at("code"), static_cast<int>(code));
    }
}

TEST(RoleManagementTest, IguanaRoleDtoRetainsCountsAndAuthorizationMetadata)
{
    RBAC::RoleResponse response{{"id", "custom_viewer", "Viewer", "Description", std::nullopt, "active", 2, 3, 4, 5, true},
                                "role-json"};
    std::string json;
    SZX::COMMON::Serializer::to_json(response, json);
    const auto parsed = SZX::COMMON::JsonHelper::Parse(json);
    EXPECT_TRUE(parsed.has_value());
    if (!parsed)
        return;
    EXPECT_EQ(parsed->at("data").at("user_count"), 4);
    EXPECT_EQ(parsed->at("data").at("permission_count"), 5);
    EXPECT_TRUE(parsed->at("data").at("manageable").get<bool>());
    EXPECT_TRUE(parsed->at("data").at("system_key").is_null());
    EXPECT_EQ(parsed->at("data").at("authz_version"), 3);
}
