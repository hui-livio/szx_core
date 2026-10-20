#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <thread>

#include "../../tools/szx_admin/src/auth/LoginService.h"
#include "../../tools/szx_admin/src/system/SystemMetrics.h"
#include "../../tools/szx_admin/src/system/SystemMetricsHandler.h"
#include "../support/HttpClient.h"
#include "JsonHelper.hpp"
#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Infra/ErrorCode.h"
#include "SzxAdmin/Infra/HttpTransport.h"
#include "SzxAdmin/Infra/RestApi.h"
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
    const auto unavailable = service.snapshot();
    EXPECT_FALSE(unavailable.ok);
    EXPECT_EQ(unavailable.error, static_cast<int>(SZX::ADMIN::INFRA::ErrorCode::ServiceUnavailable));
    const auto started = service.start();
    EXPECT_TRUE(started.ok);
    if (!started.ok)
        return;
    auto snapshot = service.snapshot();
    EXPECT_TRUE(snapshot.ok);
    EXPECT_EQ(snapshot.value.sequence, 1);
    EXPECT_EQ(snapshot.value.cpu.status, "warming_up");
    EXPECT_EQ(snapshot.value.memory.status, "ok");
    EXPECT_FALSE(snapshot.value.cpu.error_code.has_value());
    EXPECT_FALSE(snapshot.value.memory.error_code.has_value());
    EXPECT_FALSE(snapshot.value.cpu.usage_percent.has_value());
    EXPECT_LE(snapshot.value.disks.size(), 32U);
    EXPECT_FALSE(snapshot.value.cpu_name.empty());
    EXPECT_EQ(snapshot.value.gpu_count == 0, snapshot.value.gpu_name.empty());
    EXPECT_EQ(snapshot.value.gpu_count == 0, snapshot.value.gpu.status == "unavailable");
    if (snapshot.value.cpu.temperature_celsius)
        EXPECT_GE(*snapshot.value.cpu.temperature_celsius, 0.0);
    for (const auto &network : snapshot.value.networks)
    {
        EXPECT_FALSE(network.id.empty());
        if (network.rx_bytes_per_sec)
            EXPECT_GE(*network.rx_bytes_per_sec, 0.0);
    }
    const auto instance  = snapshot.value.instance_id;
    const auto duplicate = service.start();
    EXPECT_FALSE(duplicate.ok);
    EXPECT_EQ(duplicate.error, static_cast<int>(SZX::ADMIN::INFRA::ErrorCode::InvalidConfiguration));
    for (unsigned window : {0U, 901U, std::numeric_limits<unsigned>::max()})
    {
        const auto invalid = service.history(window);
        EXPECT_FALSE(invalid.ok);
        EXPECT_EQ(invalid.error, static_cast<int>(SZX::ADMIN::INFRA::ErrorCode::ValidationError));
    }
    EXPECT_TRUE(service.history(900).ok);
    std::this_thread::sleep_for(std::chrono::milliseconds(2150));
    snapshot = service.snapshot();
    EXPECT_GE(snapshot.value.sequence, 2U);
    EXPECT_EQ(snapshot.value.cpu.status, "ok");
    const auto history = service.history(300);
    EXPECT_GE(history.value.samples.size(), 2U);
    EXPECT_EQ(history.value.instance_id, instance);
    EXPECT_EQ(history.value.samples.back().gpu.status, snapshot.value.gpu.status);
    service.stop();
    const auto stopped = service.snapshot();
    EXPECT_FALSE(stopped.ok);
    EXPECT_EQ(stopped.error, static_cast<int>(SZX::ADMIN::INFRA::ErrorCode::ServiceUnavailable));
    EXPECT_TRUE(service.start().ok);
    EXPECT_NE(service.snapshot().value.instance_id, instance);
    service.stop();
}
TEST(SystemMetricsTest, ConfigRejectsUnboundedAndRelativePaths)
{
    for (auto config : std::vector<MetricsConfig>{{{"relative"}},
                                                  {{""}},
                                                  {std::vector<std::string>(33, "/")},
                                                  {{}, {""}},
                                                  {{}, std::vector<std::string>(17, "example")}})
    {
        SystemMetricsService service{std::move(config)};
        const auto started = service.start();
        EXPECT_FALSE(started.ok);
        EXPECT_EQ(started.error, static_cast<int>(SZX::ADMIN::INFRA::ErrorCode::InvalidConfiguration));
    }
}
TEST(SystemMetricsTest, ConfiguredProcessNamesAreExposedWithoutClientInput)
{
    MetricsConfig config;
    config.processNames = {"szx_missing_process_test"};
    SystemMetricsService service{std::move(config)};
    ASSERT_TRUE(service.start().ok);
    const auto snapshot = service.snapshot();
    ASSERT_TRUE(snapshot.ok);
    ASSERT_EQ(snapshot.value.processes.size(), 1U);
    EXPECT_EQ(snapshot.value.processes.front().name, "szx_missing_process_test");
    EXPECT_EQ(snapshot.value.processes.front().status, "stopped");
    EXPECT_FALSE(snapshot.value.processes.front().cpu_percent.has_value());
    service.stop();
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


TEST(SystemMetricsTest, IguanaSnapshotPreservesWireFieldsNullsAndEscapedPaths)
{
    using SZX::COMMON::JsonHelper;
    using SZX::COMMON::Serializer::to_json;
    const auto maximum     = std::numeric_limits<std::uint64_t>::max();
    const std::string path = "C:\\data\\资料\\\"report\"";
    MetricsSnapshotDto snapshot;
    snapshot.sequence                = maximum;
    snapshot.sampled_at              = 1780000000123LL;
    snapshot.instance_id             = "instance-json";
    snapshot.platform                = "windows";
    snapshot.logical_cpus            = 128;
    snapshot.cpu                     = {"ok", std::nullopt, 0.0};
    snapshot.cpu.temperature_celsius = 62.5;
    snapshot.memory                  = {"ok", std::nullopt, 100.0, maximum, maximum, 0, std::nullopt};
    snapshot.disks.push_back({"disk-json", path, 1780000000000LL, {"unavailable", 2216}});
    snapshot.disks.back().type = "SSD(NVMe)";
    snapshot.cpu_name          = "Test CPU";
    snapshot.memory_spec       = "DDR5";
    snapshot.gpu               = {"ok", std::nullopt, 72.0, maximum, maximum / 2, maximum / 2, std::nullopt, 68.0};
    snapshot.gpu_engine        = {"ok", std::nullopt, 72.0, 42.0, std::nullopt};
    snapshot.gpu_count         = 1;
    snapshot.gpu_name          = "Test GPU";
    snapshot.networks.push_back({"nic-1", "Ethernet", "Adapter", "192.0.2.5", "", 1780000000000LL, 1024.0, std::nullopt, 1000.0});
    snapshot.processes.push_back({"inspector", "stopped", std::nullopt, std::nullopt, std::nullopt});
    MetricsSnapshotResponse envelope{std::move(snapshot), "metrics-json"};
    std::string body;
    to_json(envelope, body);
    const auto parsed = JsonHelper::Parse(body);
    EXPECT_TRUE(parsed.has_value());
    if (!parsed)
        return;
    EXPECT_EQ(parsed->size(), 2U);
    EXPECT_EQ(parsed->at("request_id"), "metrics-json");
    const auto &data = parsed->at("data");
    EXPECT_EQ(data.size(), 18U);
    EXPECT_EQ(data.at("sequence").get<std::uint64_t>(), maximum);
    EXPECT_EQ(data.at("sampled_at").get<std::int64_t>(), 1780000000123LL);
    EXPECT_EQ(data.at("instance_id"), "instance-json");
    EXPECT_EQ(data.at("platform"), "windows");
    EXPECT_EQ(data.at("logical_cpus"), 128);
    EXPECT_EQ(data.at("sample_interval_ms"), 2000);
    EXPECT_EQ(data.at("disk_interval_ms"), 30000);
    EXPECT_EQ(data.at("cpu").size(), 8U);
    EXPECT_EQ(data.at("cpu").at("usage_percent"), 0.0);
    EXPECT_EQ(data.at("cpu").at("temperature_celsius"), 62.5);
    EXPECT_TRUE(data.at("cpu").at("error_code").is_null());
    EXPECT_TRUE(data.at("cpu").at("total_bytes").is_null());
    EXPECT_TRUE(data.at("cpu").at("used_bytes").is_null());
    EXPECT_TRUE(data.at("cpu").at("available_bytes").is_null());
    EXPECT_TRUE(data.at("cpu").at("free_bytes").is_null());
    EXPECT_EQ(data.at("memory").at("total_bytes").get<std::uint64_t>(), maximum);
    EXPECT_EQ(data.at("memory").at("available_bytes"), 0);
    EXPECT_TRUE(data.at("memory").at("free_bytes").is_null());
    EXPECT_EQ(data.at("disks").size(), 1U);
    EXPECT_EQ(data.at("disks").at(0).at("path"), path);
    EXPECT_EQ(data.at("disks").at(0).at("sampled_at"), 1780000000000LL);
    EXPECT_EQ(data.at("disks").at(0).at("type"), "SSD(NVMe)");
    EXPECT_EQ(data.at("cpu_name"), "Test CPU");
    EXPECT_EQ(data.at("memory_spec"), "DDR5");
    EXPECT_EQ(data.at("gpu_name"), "Test GPU");
    EXPECT_EQ(data.at("gpu_count"), 1);
    EXPECT_EQ(data.at("gpu").at("temperature_celsius"), 68.0);
    EXPECT_EQ(data.at("gpu_engine").at("cuda_percent"), 42.0);
    EXPECT_TRUE(data.at("gpu_engine").at("graphics_3d_percent").is_null());
    EXPECT_EQ(data.at("networks").at(0).at("rx_bytes_per_sec"), 1024.0);
    EXPECT_TRUE(data.at("networks").at(0).at("tx_bytes_per_sec").is_null());
    EXPECT_EQ(data.at("processes").at(0).at("status"), "stopped");
    EXPECT_TRUE(data.at("processes").at(0).at("cpu_percent").is_null());
    const auto &unavailable = data.at("disks").at(0).at("capacity");
    EXPECT_EQ(unavailable.at("status"), "unavailable");
    EXPECT_EQ(unavailable.at("error_code"), 2216);
    EXPECT_TRUE(unavailable.at("usage_percent").is_null());
    MetricsSnapshotResponse roundTrip;
    SZX::COMMON::Serializer::from_json(roundTrip, body);
    EXPECT_EQ(roundTrip.data.disks.size(), 1U);
    if (roundTrip.data.disks.empty())
        return;
    EXPECT_EQ(roundTrip.data.disks.front().path, path);
    EXPECT_EQ(roundTrip.data.sequence, maximum);
    EXPECT_EQ(roundTrip.data.networks.front().name, "Ethernet");
    EXPECT_EQ(roundTrip.data.gpu_engine.cuda_percent, 42.0);
    const auto response = SZX::ADMIN::INFRA::RestApi::serializedSuccess(std::move(body), envelope.request_id);
    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(response.headers.at("Cache-Control"), "no-store");
}

TEST(SystemMetricsTest, IguanaHistoryPreservesNestedSamplesAndEmptyArrays)
{
    MetricsHistoryResponse envelope{{"instance-history", 2000, 900, {}}, "history-json"};
    envelope.data.samples.push_back({1,
                                     1780000000000LL,
                                     {"warming_up", std::nullopt},
                                     {"ok", std::nullopt, 25.0, 4096, 1024, 3072},
                                     {"warming_up", std::nullopt}});
    envelope.data.samples.push_back(
        {2, 1780000002000LL, {"ok", std::nullopt, 0.0}, {"unavailable", 2216}, {"ok", std::nullopt, 55.0}});
    std::string body;
    SZX::COMMON::Serializer::to_json(envelope, body);
    const auto parsed = SZX::COMMON::JsonHelper::Parse(body);
    EXPECT_TRUE(parsed.has_value());
    if (!parsed)
        return;
    const auto &data = parsed->at("data");
    EXPECT_EQ(parsed->at("request_id"), "history-json");
    EXPECT_EQ(data.size(), 4U);
    EXPECT_EQ(data.at("window_seconds"), 900);
    EXPECT_EQ(data.at("samples").size(), 2U);
    EXPECT_EQ(data.at("samples").at(0).size(), 5U);
    EXPECT_EQ(data.at("samples").at(1).at("gpu").at("usage_percent"), 55.0);
    EXPECT_EQ(data.at("samples").at(0).at("cpu").at("status"), "warming_up");
    EXPECT_TRUE(data.at("samples").at(0).at("cpu").at("error_code").is_null());
    EXPECT_TRUE(data.at("samples").at(0).at("cpu").at("usage_percent").is_null());
    EXPECT_EQ(data.at("samples").at(1).at("cpu").at("usage_percent"), 0.0);
    EXPECT_EQ(data.at("samples").at(1).at("memory").at("error_code"), 2216);
    MetricsHistoryResponse roundTrip;
    SZX::COMMON::Serializer::from_json(roundTrip, body);
    EXPECT_EQ(roundTrip.data.samples.size(), 2U);
    if (roundTrip.data.samples.size() != 2)
        return;
    EXPECT_EQ(roundTrip.data.samples[1].sequence, 2U);
    envelope.data.samples.clear();
    body.clear();
    SZX::COMMON::Serializer::to_json(envelope, body);
    const auto emptyHistory = SZX::COMMON::JsonHelper::Parse(body);
    EXPECT_TRUE(emptyHistory.has_value());
    if (!emptyHistory)
        return;
    EXPECT_TRUE(emptyHistory->at("data").at("samples").is_array());
    EXPECT_TRUE(emptyHistory->at("data").at("samples").empty());
    MetricsSnapshotResponse emptySnapshot;
    body.clear();
    SZX::COMMON::Serializer::to_json(emptySnapshot, body);
    const auto emptyDisks = SZX::COMMON::JsonHelper::Parse(body);
    EXPECT_TRUE(emptyDisks.has_value());
    if (!emptyDisks)
        return;
    EXPECT_TRUE(emptyDisks->at("data").at("disks").is_array());
    EXPECT_TRUE(emptyDisks->at("data").at("disks").empty());
}
