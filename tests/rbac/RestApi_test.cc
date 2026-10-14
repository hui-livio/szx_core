#include "SzxAdmin/Infra/RestApi.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>
#include <vector>

using namespace SZX::ADMIN::INFRA;

TEST(RestApiTest, ParsesJsonContentTypeAndObjectStrictly)
{
    for (const auto contentType : {"application/json", "Application/JSON; Charset=UTF-8", "application/json;charset=\"utf-8\""})
    {
        SCOPED_TRACE(contentType);
        HttpRequest request{.body = R"({"name":"Alice"})", .headers = {{"content-type", contentType}}};
        const auto parsed = RestApi::jsonObject(request);
        ASSERT_TRUE(parsed.ok);
        EXPECT_EQ(parsed.value.at("name"), "Alice");
    }

    const std::vector<std::pair<std::string, std::string>> invalidRequests{
        {"", R"({"name":"Alice"})"},
        {"text/plain", R"({"name":"Alice"})"},
        {"application/json; charset=gbk", R"({"name":"Alice"})"},
        {"application/json; charset=utf-8; x=y", R"({"name":"Alice"})"},
        {"application/json", "[1]"},
        {"application/json", "{"}};
    for (const auto &[contentType, body] : invalidRequests)
    {
        SCOPED_TRACE(contentType + body);
        HttpRequest request{.body = body};
        if (!contentType.empty())
            request.headers.emplace("Content-Type", contentType);
        const auto parsed = RestApi::jsonObject(request);
        EXPECT_FALSE(parsed.ok);
        EXPECT_EQ(parsed.error, static_cast<int>(ErrorCode::ValidationError));
    }
}

TEST(RestApiTest, RejectsFieldsOutsideControllerAllowlist)
{
    const std::array<std::string_view, 2> allowed{"identifier", "password"};
    EXPECT_TRUE(RestApi::allowedFields(SZX::COMMON::Json{{"identifier", "admin"}, {"password", "secret"}}, allowed).ok);
    const auto extra = RestApi::allowedFields(
        SZX::COMMON::Json{{"identifier", "admin"}, {"password", "secret"}, {"role", "super_admin"}}, allowed);
    EXPECT_FALSE(extra.ok);
    EXPECT_EQ(extra.error, static_cast<int>(ErrorCode::ValidationError));
}

TEST(RestApiTest, ParsesBoundedPagination)
{
    HttpRequest request;
    auto parsed = RestApi::pagination(request);
    ASSERT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.value.page, 1U);
    EXPECT_EQ(parsed.value.pageSize, 20U);
    EXPECT_EQ(parsed.value.offset(), 0U);

    request.query = {{"page", "3"}, {"page_size", "100"}};
    parsed        = RestApi::pagination(request);
    ASSERT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.value.offset(), 200U);

    request.query = {{"page", "4294967295"}, {"page_size", "100"}};
    parsed        = RestApi::pagination(request);
    ASSERT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.value.offset(), 429496729400ULL);
    request.query = {{"page", "4294967296"}};
    EXPECT_FALSE(RestApi::pagination(request).ok);

    for (const auto value : {"", "0", "-1", "+1", "1x", "101", "99999999999"})
    {
        SCOPED_TRACE(value);
        request.query = {{"page_size", value}};
        parsed        = RestApi::pagination(request);
        EXPECT_FALSE(parsed.ok);
        EXPECT_EQ(parsed.error, static_cast<int>(ErrorCode::ValidationError));
    }
}

TEST(RestApiTest, ParsesStrongIfMatchVersion)
{
    EXPECT_FALSE(RestApi::ifMatch(HttpRequest{}).ok);
    for (const auto value : {"1", "2147483647", "\"42\""})
    {
        SCOPED_TRACE(value);
        HttpRequest request{.headers = {{"if-match", value}}};
        const auto version = RestApi::ifMatch(request);
        ASSERT_TRUE(version.ok);
        EXPECT_GT(version.value, 0U);
    }
    for (const auto value : {"", "0", "*", "W/\"1\"", "\"1\",\"2\"", "2147483648"})
    {
        SCOPED_TRACE(value);
        HttpRequest request{.headers = {{"If-Match", value}}};
        const auto version = RestApi::ifMatch(request);
        EXPECT_FALSE(version.ok);
        EXPECT_EQ(version.error, static_cast<int>(ErrorCode::ValidationError));
    }
}

TEST(RestApiTest, ParsesBoundedBearerCredential)
{
    EXPECT_FALSE(RestApi::bearerToken(HttpRequest{}).ok);
    EXPECT_FALSE(RestApi::bearerToken(HttpRequest{.headers = {{"Authorization", "Bearer " + std::string(8192, 'a')}}}).ok);
    HttpRequest request{.headers = {{"authorization", "bEaReR header.payload.signature"}}};
    auto token = RestApi::bearerToken(request);
    ASSERT_TRUE(token.ok);
    EXPECT_EQ(token.value, "header.payload.signature");

    request.headers = {{"Authorization", "Bearer   abc+/=="}};
    token           = RestApi::bearerToken(request);
    ASSERT_TRUE(token.ok);
    EXPECT_EQ(token.value, "abc+/==");

    for (const auto value : {"", "Basic secret", "Bearer", "Bearer ", "Bearer two tokens", "Bearer token\nvalue", "Bearer abc?",
                             "Bearer a=b", "Bearer =="})
    {
        SCOPED_TRACE(value);
        request.headers = {{"Authorization", value}};
        token           = RestApi::bearerToken(request);
        EXPECT_FALSE(token.ok);
        EXPECT_EQ(token.error, static_cast<int>(ErrorCode::AuthRequired));
    }
}

TEST(RestApiTest, ProducesStableSuccessAndErrorEnvelopes)
{
    const auto success = RestApi::success(SZX::COMMON::Json{{"id", "user-1"}}, "request-1", 201);
    EXPECT_EQ(success.statusCode, 201);
    EXPECT_EQ(success.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(success.headers.at("Cache-Control"), "no-store");
    const auto successJson = SZX::COMMON::Json::parse(success.body);
    EXPECT_EQ(successJson.at("data").at("id"), "user-1");
    EXPECT_EQ(successJson.at("request_id"), "request-1");

    const auto error = RestApi::error(ErrorCode::ConflictVersion, "request-2");
    EXPECT_EQ(error.statusCode, 409);
    EXPECT_EQ(error.headers.at("Cache-Control"), "no-store");
    const auto errorJson = SZX::COMMON::Json::parse(error.body);
    EXPECT_EQ(errorJson.at("code"), 2209);
    EXPECT_TRUE(errorJson.at("details").empty());
    EXPECT_EQ(errorJson.at("request_id"), "request-2");

    const auto unknown = RestApi::error(static_cast<ErrorCode>(-1), "request-3");
    EXPECT_EQ(unknown.statusCode, 500);
    EXPECT_EQ(SZX::COMMON::Json::parse(unknown.body).at("code"), 2299);

    const auto unauthorized = RestApi::error(ErrorCode::AuthRequired, "request-4");
    EXPECT_EQ(unauthorized.statusCode, 401);
    EXPECT_EQ(unauthorized.headers.at("WWW-Authenticate"), "Bearer");

    const auto empty = RestApi::noContent("request-4");
    EXPECT_EQ(empty.statusCode, 204);
    EXPECT_TRUE(empty.body.empty());
}


TEST(RestApiTest, SerializedSuccessPreservesIguanaBodyAndStatusPolicy)
{
    const std::string body = "  {\"data\":{\"value\":null},\"request_id\":\"serialized-request\"}  ";
    for (const auto status : {200, 201, 202, 204, 400, 500})
    {
        SCOPED_TRACE(status);
        const auto response = RestApi::serializedSuccess(body, "serialized-request", status);
        if (status == 200 || status == 201 || status == 202)
        {
            EXPECT_EQ(response.statusCode, status);
            EXPECT_EQ(response.body, body); // No parse/dump or field rebuilding.
            EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
            EXPECT_EQ(response.headers.at("Cache-Control"), "no-store");
            EXPECT_EQ(response.headers.at("X-Request-ID"), "serialized-request");
        }
        else
        {
            EXPECT_EQ(response.statusCode, 500);
            EXPECT_EQ(SZX::COMMON::Json::parse(response.body).at("code"), 2299);
        }
    }
}

TEST(RestApiTest, PreservesSecondPhasePermissionErrorCodes)
{
    for (const auto code : {ErrorCode::PermissionCodeConflict, ErrorCode::PermissionInUse, ErrorCode::ProtectedPermission,
                            ErrorCode::SuperAdminSecurityFloor})
    {
        SCOPED_TRACE(static_cast<int>(code));
        const auto response = RestApi::error(code, "rbac-error");
        EXPECT_EQ(response.statusCode, 409);
        const auto parsed = SZX::COMMON::Json::parse(response.body);
        EXPECT_EQ(parsed.at("code"), static_cast<int>(code));
        EXPECT_FALSE(parsed.at("message").get<std::string>().empty());
        EXPECT_NE(errorCodeName(code), "INTERNAL_FAILURE");
    }
}
