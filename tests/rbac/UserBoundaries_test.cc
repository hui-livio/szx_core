#include <gtest/gtest.h>

#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

#include "../../RBAC/szx_admin/src/HttpSupport.h"
#include "../../RBAC/szx_admin/src/RateLimiter.h"
#include "../../RBAC/szx_admin/src/user/AuditDetails.h"

TEST(AdminOriginTest, ExactOriginRestrictsSourcesAndWildcardAcceptsHttpOrigins)
{
    using namespace SZX::ADMIN;
    INFRA::HttpRequest request;
    constexpr auto allowed = "https://admin.example.test";
    EXPECT_TRUE(originAllowed(allowed, request)); // Native clients still require Bearer/CSRF credentials.
    for (const auto origin : {"null", "*", "https://evil.test", "https://admin.example.test.evil.test",
                              "http://admin.example.test", "https://admin.example.test:8443", ""})
    {
        request.headers["Origin"] = origin;
        EXPECT_FALSE(originAllowed(allowed, request)) << origin;
        const auto response = preflightHandler(allowed, "POST, OPTIONS")(request);
        EXPECT_EQ(response.statusCode, 403);
        EXPECT_FALSE(response.headers.contains("Access-Control-Allow-Origin"));
    }
    request.headers["Origin"] = allowed;
    EXPECT_TRUE(originAllowed(allowed, request));
    EXPECT_TRUE(originAllowed("*", request));
    const auto accepted = preflightHandler(allowed, "POST, OPTIONS")(request);
    EXPECT_EQ(accepted.statusCode, 204);
    EXPECT_EQ(accepted.headers.at("Access-Control-Allow-Origin"), allowed);
    for (const auto origin : {"http://localhost:5173", "https://other.example.test"})
    {
        request.headers["Origin"] = origin;
        const auto response       = preflightHandler("*", "POST, OPTIONS")(request);
        EXPECT_EQ(response.statusCode, 204);
        EXPECT_EQ(response.headers.at("Access-Control-Allow-Origin"), origin);
        EXPECT_EQ(response.headers.at("Access-Control-Allow-Credentials"), "true");
        EXPECT_EQ(response.headers.at("Vary"), "Origin");
    }
    for (const auto origin : {"null", "", "*", "file://local", "https://", "https://user@host", "https://host/path"})
    {
        request.headers["Origin"] = origin;
        const auto response       = preflightHandler("*", "POST, OPTIONS")(request);
        EXPECT_EQ(response.statusCode, 403);
        EXPECT_FALSE(response.headers.contains("Access-Control-Allow-Origin"));
    }
}

TEST(AdminResponseTimeTest, ConvertsUtcSecondsAndMicrosecondsToIntegerMilliseconds)
{
    using SZX::ADMIN::utcTimestampJson;
    constexpr std::int64_t expected = 1789636500000;
    EXPECT_EQ(utcTimestampJson(std::string{"2026-09-17T09:15:00Z"}), expected);
    EXPECT_EQ(utcTimestampJson(std::string{"2026-09-17 09:15:00"}), expected);
    EXPECT_EQ(utcTimestampJson(std::string{"2026-09-17 09:15:00.5"}), expected + 500);
    EXPECT_EQ(utcTimestampJson(std::string{"2026-09-17 09:15:00.510480"}), expected + 510);
    EXPECT_EQ(utcTimestampJson(std::string{"2026-09-17 09:15:00.000999"}), expected);
    EXPECT_TRUE(utcTimestampJson(std::string{"2026-09-17 09:15:00"}).is_number_integer());
    EXPECT_EQ(utcTimestampJson(std::string{"1970-01-01 00:00:00"}), 0);
    EXPECT_TRUE(utcTimestampJson(std::nullopt).is_null());
}

TEST(AdminResponseTimeTest, RejectsInvalidPersistedTimestampsInsteadOfReturningEpochZero)
{
    for (const std::string input : {"", "invalid", "2026-02-30 09:15:00", "2026-09-17 09:15:00.", "2026-09-17 09:15:00.1234567",
                                    "2026-09-17 09:15:00.123x", "2026-09-17T09:15:00+08:00"})
    {
        SCOPED_TRACE(input);
        EXPECT_THROW((void)SZX::ADMIN::utcTimestampJson(input), std::invalid_argument);
    }
}

TEST(AdminAuditTest, RecursivelyRedactsSensitiveFieldsWithoutLosingSafeMetadata)
{
    using SZX::COMMON::Json;
    Json details{{"source", "dashboard"},
                 {"Password", "secret"},
                 {"private-key", "secret"},
                 {"credentials", {{"value", "secret"}}},
                 {"context", {{"access_token", "secret"}, {"Machine-ID", "secret"}, {"count", 3}}},
                 {"items",
                  Json::array({Json{{"Authorization", "secret"}, {"source", "desktop"}},
                               Json{{"stack_trace", "secret"}, {"api_key", "secret"}}})}};
    SZX::ADMIN::USER::redactAuditDetails(details);
    const Json expected{{"source", "dashboard"},
                        {"context", {{"count", 3}}},
                        {"items", Json::array({Json{{"source", "desktop"}}, Json::object()})}};
    EXPECT_EQ(details, expected);
    EXPECT_EQ(details.dump().find("secret"), std::string::npos);
}

TEST(AdminRateLimitTest, BoundsWindowsExpiresAndKeepsOperationFamiliesIndependent)
{
    using SZX::ADMIN::RateLimiter;
    const auto now = RateLimiter::Clock::now();
    RateLimiter auth(2);
    RateLimiter behavior(2);
    EXPECT_TRUE(behavior.admit("user", now));
    EXPECT_TRUE(behavior.admit("user", now));
    EXPECT_FALSE(behavior.admit("user", now));
    EXPECT_TRUE(auth.admit("user", now));
    EXPECT_TRUE(behavior.admit("other", now));
    EXPECT_TRUE(behavior.admit("user", now + std::chrono::minutes{1}));

    RateLimiter bounded(1);
    for (int index = 0; index < 4096; ++index)
        ASSERT_TRUE(bounded.admit(std::to_string(index), now));
    EXPECT_FALSE(bounded.admit("overflow", now));
    EXPECT_TRUE(bounded.admit("overflow", now + std::chrono::minutes{1}));
}

TEST(AdminRateLimitTest, ConcurrentRequestsCannotExceedTheBudget)
{
    SZX::ADMIN::RateLimiter limiter(5);
    const auto now = SZX::ADMIN::RateLimiter::Clock::now();
    std::atomic<int> admitted{};
    std::vector<std::jthread> requests;
    for (int index = 0; index < 20; ++index)
        requests.emplace_back(
            [&]
            {
                if (limiter.admit("user", now))
                    ++admitted;
            });
    requests.clear();
    EXPECT_EQ(admitted.load(), 5);
}
