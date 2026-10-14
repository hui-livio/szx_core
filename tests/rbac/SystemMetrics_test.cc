#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <thread>

#include "../../tools/szx_admin/src/auth/LoginService.h"
#include "../../tools/szx_admin/src/system/SystemMetrics.h"
#include "../../tools/szx_admin/src/system/SystemMetricsHandler.h"
#include "../support/HttpClient.h"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Policy/Policy.h"

using namespace SZX::ADMIN::SYSTEM;

TEST(SystemMetricsTest, CpuDifferentialAndRollback)
{
    struct Case
    {
        CpuCounters before;
        CpuCounters after;
        std::optional<double> want;
    };
    const std::vector<Case> cases{{{100, 20}, {200, 70}, 50.0}, {{100, 20}, {200, 120}, 0.0}, {{100, 20}, {200, 20}, 100.0},
                                  {{100, 20}, {100, 20}, {}},   {{200, 20}, {100, 20}, {}},   {{100, 30}, {200, 20}, {}},
                                  {{100, 20}, {110, 40}, {}}};
    for (const auto &tc : cases)
    {
        SCOPED_TRACE(tc.after.total);
        EXPECT_EQ(cpuUsage(tc.before, tc.after), tc.want);
    }
}
TEST(SystemMetricsTest, ProcParsersRejectMissingAndInvalidData)
{
    const auto cpu = parseCpuCounters("cpu 10 20 30 40 5 6 7 8 999 888\ncpu0 1 2 3 4");
    EXPECT_TRUE(cpu.has_value());
    if (cpu)
    {
        EXPECT_EQ(cpu->total, 126U);
        EXPECT_EQ(cpu->idle, 45U);
    }
    for (const auto &text : {"", "cpu0 1 2 3 4 5 6 7 8", "cpu 1 2 bad", "cpu 18446744073709551615 1 1 1 1 1 1 1"})
        EXPECT_FALSE(parseCpuCounters(text).has_value());
    const auto memory = parseMemory("MemTotal: 100 kB\nMemFree: 5 kB\nMemAvailable: 40 kB\nCached: 30 kB\n");
    EXPECT_EQ(memory.status, "ok");
    EXPECT_EQ(memory.totalBytes, 102400U);
    EXPECT_EQ(memory.usedBytes, 61440U);
    EXPECT_EQ(memory.availableBytes, 40960U);
    EXPECT_EQ(memory.usagePercent, 60.0);
    EXPECT_FALSE(memory.freeBytes);
    for (const auto &text : {"", "MemTotal: 100 kB", "MemTotal: 0 kB\nMemAvailable: 0 kB", "MemTotal: 10 kB\nMemAvailable: 20 kB",
                             "MemTotal: 10 MB\nMemAvailable: 5 kB"})
        EXPECT_EQ(parseMemory(text).status, "unavailable");
}
TEST(SystemMetricsTest, CapacityKeepsQuotaAvailableSeparate)
{
    const auto disk = capacityMetric(1000, 400, 300);
    EXPECT_EQ(disk.status, "ok");
    EXPECT_EQ(disk.usedBytes, 600U);
    EXPECT_EQ(disk.freeBytes, 400U);
    EXPECT_EQ(disk.availableBytes, 300U);
    for (const auto &args : std::vector<std::array<std::uint64_t, 3>>{{0, 0, 0}, {1, 2, 1}, {100, 10, 20}})
        EXPECT_EQ(capacityMetric(args[0], args[1], args[2]).status, "unavailable");
}
TEST(SystemMetricsTest, RealPlatformLifecycleAndBoundedHistory)
{
    SystemMetricsService service;
    EXPECT_FALSE(service.snapshot().ok);
    const auto started = service.start();
    EXPECT_TRUE(started.ok);
    if (!started.ok)
        return;
    auto snapshot = service.snapshot();
    EXPECT_TRUE(snapshot.ok);
    EXPECT_EQ(snapshot.value["sequence"], 1);
    EXPECT_EQ(snapshot.value["cpu"]["status"], "warming_up");
    EXPECT_EQ(snapshot.value["memory"]["status"], "ok");
    EXPECT_TRUE(snapshot.value["cpu"]["usage_percent"].is_null());
    EXPECT_LE(snapshot.value["disks"].size(), 32U);
    const auto instance = snapshot.value["instance_id"];
    EXPECT_FALSE(service.start().ok);
    for (unsigned window : {0U, 901U, std::numeric_limits<unsigned>::max()})
        EXPECT_FALSE(service.history(window).ok);
    EXPECT_TRUE(service.history(900).ok);
    std::this_thread::sleep_for(std::chrono::milliseconds(2150));
    snapshot = service.snapshot();
    EXPECT_GE(snapshot.value["sequence"].get<unsigned>(), 2U);
    EXPECT_EQ(snapshot.value["cpu"]["status"], "ok");
    const auto history = service.history(300);
    EXPECT_GE(history.value["samples"].size(), 2U);
    EXPECT_EQ(history.value["instance_id"], instance);
    service.stop();
    EXPECT_FALSE(service.snapshot().ok);
    EXPECT_TRUE(service.start().ok);
    EXPECT_NE(service.snapshot().value["instance_id"], instance);
    service.stop();
}
TEST(SystemMetricsTest, ConfigRejectsUnboundedAndRelativePaths)
{
    for (auto config : std::vector<MetricsConfig>{{{"relative"}}, {{""}}, {std::vector<std::string>(33, "/")}})
    {
        SystemMetricsService service{std::move(config)};
        EXPECT_FALSE(service.start().ok);
    }
}
TEST(SystemMetricsTest, OnlyAdminPolicyHasMetricsRead)
{
    using namespace SZX::ADMIN::POLICY;
    for (const auto role : {"operator", "engineer", "admin", "super_admin", "unknown"})
        EXPECT_EQ(allows(role, Capability::SystemMetricsRead),
                  std::string_view(role) == "admin" || std::string_view(role) == "super_admin");
}
TEST(SystemMetricsTest, HttpRoutesRequireAuthenticationAndApplyCors)
{
    using namespace SZX::ADMIN::INFRA;
    auto reservation = SZX::NETWORK::HttpServer::Create({});
    EXPECT_TRUE(reservation.ok);
    if (!reservation.ok)
        return;
    EXPECT_TRUE(reservation.value->start().ok);
    ServerConfig config;
    config.port = reservation.value->port();
    config.staticRoot.clear();
    EXPECT_TRUE(reservation.value->stop().ok);
    DatabasePool database{DatabaseConfig{}};
    SZX::ADMIN::AUTH::LoginService login{database, {}};
    SystemMetricsService metrics;
    HttpTransport transport{config};
    EXPECT_TRUE(registerSystemMetricsRoutes(transport, database, login, metrics).ok);
    EXPECT_TRUE(transport.start().ok);
    for (const auto path : {"/api/v1/admin/system/metrics", "/api/v1/admin/system/metrics/history?window_seconds=0"})
    {
        SZX::TEST::HTTP::Client client{transport.port()};
        client.send(std::string("GET ") + path +
                    " HTTP/1.1\r\nHost: localhost\r\nOrigin: https://example.org\r\nConnection: close\r\n\r\n");
        const auto response = client.read();
        EXPECT_NE(response.find("401"), std::string::npos);
        EXPECT_NE(response.find("2200"), std::string::npos);
        EXPECT_NE(response.find("https://example.org"), std::string::npos);
    }
    EXPECT_TRUE(transport.stop().ok);
}
