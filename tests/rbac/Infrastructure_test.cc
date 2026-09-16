#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/RequestContext.h"
#include "SzxAdmin/Infra/ServerConfig.h"

#include <gtest/gtest.h>

#include <string>

using SZX::ADMIN::INFRA::ErrorCode;
using SZX::ADMIN::INFRA::HttpResponse;
using SZX::ADMIN::INFRA::HttpTransport;
using SZX::ADMIN::INFRA::RequestContext;
using SZX::ADMIN::INFRA::ServerConfig;

TEST(ServerConfigTest, AcceptsInjectedConfiguration)
{
    const ServerConfig config{.host                = "127.0.0.1",
                              .port                = 18080,
                              .workerThreads       = 2,
                              .maxRequestBodyBytes = 4096,
                              .maxConnections      = 256,
                              .apiPrefix           = "/api/v2",
                              .accessLog           = true};

    const auto result = config.validate();
    EXPECT_TRUE(result.ok) << result.message;
}

TEST(ServerConfigTest, RejectsInvalidInjectedConfiguration)
{
    ServerConfig config;
    config.port       = 0;
    const auto result = config.validate();
    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(ErrorCode::InvalidConfiguration));
}

TEST(RequestContextTest, PreservesSafeRequestId)
{
    const auto context = RequestContext::create("request-2026.09");
    EXPECT_EQ(context.requestId, "request-2026.09");
}

TEST(RequestContextTest, ReplacesInvalidRequestId)
{
    const auto context = RequestContext::create("request id");
    EXPECT_NE(context.requestId, "request id");
    EXPECT_TRUE(SZX::ADMIN::INFRA::isValidRequestId(context.requestId));
}

TEST(HttpErrorCodeTest, MapsContractCodesToStatusAndNames)
{
    EXPECT_EQ(SZX::ADMIN::INFRA::errorCodeHttpStatus(ErrorCode::AuthRequired), 401);
    EXPECT_EQ(SZX::ADMIN::INFRA::errorCodeHttpStatus(ErrorCode::ConflictVersion), 409);
    EXPECT_EQ(SZX::ADMIN::INFRA::errorCodeHttpStatus(ErrorCode::PayloadTooLarge), 413);
    EXPECT_EQ(SZX::ADMIN::INFRA::errorCodeName(ErrorCode::Forbidden), "FORBIDDEN");
}

TEST(HttpTransportTest, BuildsHealthResponseWithRequestId)
{
    const auto response = HttpTransport::healthResponse("request-1");
    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
    EXPECT_EQ(response.headers.at("X-Request-ID"), "request-1");
    EXPECT_NE(response.body.find("\"status\":\"ok\""), std::string::npos);
    EXPECT_NE(response.body.find("\"request_id\":\"request-1\""), std::string::npos);
}

TEST(HttpTransportTest, RejectsInvalidRouteAndProtectsHealthRoute)
{
    HttpTransport transport(ServerConfig{});
    const auto invalid = transport.registerRoute("TRACE", "/api/v1/test", [](const auto &) { return HttpResponse{}; });
    ASSERT_FALSE(invalid.ok);
    EXPECT_EQ(invalid.error, static_cast<int>(ErrorCode::ValidationError));

    for (const auto path : {"/api/v1/health", "/api/v1/health/", "//api/v1/health", "///api//v1///health///"})
    {
        SCOPED_TRACE(path);
        const auto health = transport.registerRoute("GET", path, [](const auto &) { return HttpResponse{}; });
        EXPECT_FALSE(health.ok);
        EXPECT_EQ(health.error, static_cast<int>(ErrorCode::RouteConflict));
    }
    EXPECT_TRUE(transport.registerRoute("GET", "/health", [](const auto &) { return HttpResponse{}; }).ok);
}
