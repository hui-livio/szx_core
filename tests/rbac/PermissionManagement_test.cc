#include <gtest/gtest.h>

#include "../../tools/szx_admin/src/auth/LoginService.h"
#include "../../tools/szx_admin/src/rbac/PermissionDto.h"
#include "../../tools/szx_admin/src/rbac/PermissionService.h"
#include "JsonHelper.hpp"

using namespace SZX::ADMIN;

TEST(PermissionManagementTest, RejectsReservedCodesAndInvalidMetadataBeforeStorage)
{
    INFRA::DatabasePool database{INFRA::DatabaseConfig{}};
    AUTH::LoginService login{database, {}};
    RBAC::PermissionService service{database, login};
    const USER::ActorContext actor;
    for (const auto &code : {"", "system.custom", "user.custom", "role.custom", "permission.custom", "auth.custom",
                             "audit.custom", "behavior.custom", "report..view", "REPORT.view"})
    {
        SCOPED_TRACE(code);
        const auto result = service.create(actor, {code, "Example", "", "reports"});
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error, static_cast<int>(INFRA::ErrorCode::ValidationError));
    }
    for (const auto &module : {"", "bad module", "bad\nmodule"})
    {
        SCOPED_TRACE(module);
        const auto result = service.create(actor, {"report.view", "Example", "", module});
        EXPECT_FALSE(result.ok);
        EXPECT_EQ(result.error, static_cast<int>(INFRA::ErrorCode::ValidationError));
    }
}
TEST(PermissionManagementTest, IguanaPermissionDtoIncludesDescriptionAndOptimisticVersion)
{
    RBAC::PermissionResponse response{
        {"permission-1", "report.export", "导出报表", "说明 \"引号\"", "reports", "client", "custom", "active", false, 1},
        "permission-json"};
    std::string json;
    SZX::COMMON::Serializer::to_json(response, json);
    const auto parsed = SZX::COMMON::JsonHelper::Parse(json);
    EXPECT_TRUE(parsed.has_value());
    if (!parsed)
        return;
    EXPECT_EQ(parsed->at("data").size(), 10U);
    EXPECT_EQ(parsed->at("data").at("description"), response.data.description);
    EXPECT_EQ(parsed->at("data").at("version"), 1);
    EXPECT_EQ(parsed->at("data").at("enforcement"), "client");
    EXPECT_EQ(parsed->at("data").at("source"), "custom");
    EXPECT_FALSE(parsed->at("data").at("required_for_super_admin").get<bool>());
    RBAC::PermissionListResponse list;
    json.clear();
    SZX::COMMON::Serializer::to_json(list, json);
    const auto empty = SZX::COMMON::JsonHelper::Parse(json);
    EXPECT_TRUE(empty.has_value());
    if (!empty)
        return;
    EXPECT_TRUE(empty->at("data").at("items").is_array());
    EXPECT_TRUE(empty->at("data").at("items").empty());
}
