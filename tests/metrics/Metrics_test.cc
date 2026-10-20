#include <gtest/gtest.h>

#include "SzxMetrics/Metrics.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace
{
/** 测试辅助：按库约定填写 struct_size。 */
template <typename T>
auto sized() -> T
{
    T value{};
    value.struct_size = SZX_METRICS_SIZE_OF(T);
    return value;
}
} // namespace

TEST(SzxMetricsAbi, VersionAndConfigDefaults)
{
    EXPECT_EQ(szx_metrics_abi_version(), SZX_METRICS_ABI_VERSION);
    szx_metrics_config config{};
    szx_metrics_config_init(&config);
    EXPECT_EQ(config.struct_size, SZX_METRICS_SIZE_OF(szx_metrics_config));
    EXPECT_EQ(config.flags, SZX_METRICS_CARD_ALL);
    EXPECT_EQ(config.sample_interval_ms, 0U);
    EXPECT_EQ(config.disk_interval_ms, 0U);
    EXPECT_EQ(config.history_capacity, 0U);
    EXPECT_EQ(config.disk_paths, nullptr);
    EXPECT_EQ(config.process_names, nullptr);
}
TEST(SzxMetricsAbi, CpuParsersRejectMissingAndInvalidData)
{
    szx_metrics_cpu_counters counters = sized<szx_metrics_cpu_counters>();
    const std::string valid           = "cpu 10 20 30 40 5 6 7 8 999 888\ncpu0 1 2 3 4";
    ASSERT_EQ(szx_metrics_parse_cpu_counters(valid.data(), static_cast<std::uint32_t>(valid.size()), &counters), SZX_METRICS_OK);
    EXPECT_EQ(counters.total, 126U);
    EXPECT_EQ(counters.idle, 45U);
    for (const auto &text : {"", "cpu0 1 2 3 4 5 6 7 8", "cpu 1 2 bad", "cpu 18446744073709551615 1 1 1 1 1 1 1"})
        EXPECT_NE(szx_metrics_parse_cpu_counters(text, 0, &counters), SZX_METRICS_OK);
    EXPECT_EQ(szx_metrics_parse_cpu_counters(nullptr, 0, &counters), SZX_METRICS_INVALID_ARGUMENT);
    // 结构体过小必须被拒绝，而不是写越界
    szx_metrics_cpu_counters small{};
    small.struct_size = 8;
    EXPECT_EQ(szx_metrics_parse_cpu_counters(valid.data(), 0, &small), SZX_METRICS_INVALID_ARGUMENT);
}
TEST(SzxMetricsAbi, CpuUsageDifferentialAndRollback)
{
    struct Case
    {
        szx_metrics_cpu_counters before;
        szx_metrics_cpu_counters after;
        std::int32_t expected;
        double want{};
    };
    const std::vector<Case> cases{{{0, 0, 100, 20}, {0, 0, 200, 70}, SZX_METRICS_OK, 50.0},
                                  {{0, 0, 100, 20}, {0, 0, 200, 120}, SZX_METRICS_OK, 0.0},
                                  {{0, 0, 100, 20}, {0, 0, 200, 20}, SZX_METRICS_OK, 100.0},
                                  {{0, 0, 100, 20}, {0, 0, 100, 20}, SZX_METRICS_OUT_OF_RANGE, 0.0},
                                  {{0, 0, 200, 20}, {0, 0, 100, 20}, SZX_METRICS_OUT_OF_RANGE, 0.0},
                                  {{0, 0, 100, 30}, {0, 0, 200, 20}, SZX_METRICS_OUT_OF_RANGE, 0.0},
                                  {{0, 0, 100, 20}, {0, 0, 110, 40}, SZX_METRICS_OUT_OF_RANGE, 0.0}};
    for (const auto &test : cases)
    {
        double percent = -1.0;
        EXPECT_EQ(szx_metrics_cpu_usage(&test.before, &test.after, &percent), test.expected);
        if (test.expected == SZX_METRICS_OK)
            EXPECT_DOUBLE_EQ(percent, test.want);
    }
    EXPECT_EQ(szx_metrics_cpu_usage(nullptr, nullptr, nullptr), SZX_METRICS_INVALID_ARGUMENT);
}
TEST(SzxMetricsAbi, MemoryParserSeparatesAvailableFromFree)
{
    szx_metrics_metric metric = sized<szx_metrics_metric>();
    const std::string text    = "MemTotal: 100 kB\nMemFree: 5 kB\nMemAvailable: 40 kB\nCached: 30 kB\n";
    ASSERT_EQ(szx_metrics_parse_memory(text.data(), 0, &metric), SZX_METRICS_OK);
    EXPECT_EQ(metric.state, SZX_METRICS_METRIC_OK);
    EXPECT_EQ(metric.total_bytes, 102400U);
    EXPECT_EQ(metric.used_bytes, 61440U);
    EXPECT_EQ(metric.available_bytes, 40960U);
    EXPECT_DOUBLE_EQ(metric.usage_percent, 60.0);
    EXPECT_EQ(metric.present & SZX_METRICS_FIELD_FREE, 0U);
    for (const auto &invalid : {"", "MemTotal: 100 kB", "MemTotal: 0 kB\nMemAvailable: 0 kB",
                                "MemTotal: 10 kB\nMemAvailable: 20 kB", "MemTotal: 10 MB\nMemAvailable: 5 kB"})
    {
        metric = sized<szx_metrics_metric>();
        EXPECT_EQ(szx_metrics_parse_memory(invalid, 0, &metric), SZX_METRICS_OK);
        EXPECT_EQ(metric.state, SZX_METRICS_METRIC_UNAVAILABLE);
    }
}
TEST(SzxMetricsAbi, CapacityKeepsAvailableAndFreeSeparate)
{
    szx_metrics_metric metric = sized<szx_metrics_metric>();
    ASSERT_EQ(szx_metrics_capacity_metric(1000, 400, 300, &metric), SZX_METRICS_OK);
    EXPECT_EQ(metric.state, SZX_METRICS_METRIC_OK);
    EXPECT_EQ(metric.used_bytes, 600U);
    EXPECT_EQ(metric.free_bytes, 400U);
    EXPECT_EQ(metric.available_bytes, 300U);
    for (const std::array<std::uint64_t, 3> &args :
         std::vector<std::array<std::uint64_t, 3>>{{0, 0, 0}, {1, 2, 1}, {100, 10, 20}})
    {
        metric = sized<szx_metrics_metric>();
        EXPECT_EQ(szx_metrics_capacity_metric(args[0], args[1], args[2], &metric), SZX_METRICS_OK);
        EXPECT_EQ(metric.state, SZX_METRICS_METRIC_UNAVAILABLE);
    }
}
TEST(SzxMetricsService, RejectsInvalidConfiguration)
{
    for (const auto &paths : std::vector<std::vector<std::string>>{{"relative"}, {""}, std::vector<std::string>(33, "/")})
    {
        std::vector<const char *> raw;
        for (const auto &path : paths)
            raw.push_back(path.c_str());
        szx_metrics_config config;
        szx_metrics_config_init(&config);
        config.disk_paths            = raw.data();
        config.disk_path_count       = static_cast<std::uint32_t>(raw.size());
        szx_metrics_service *service = nullptr;
        EXPECT_EQ(szx_metrics_service_create(&config, &service), SZX_METRICS_INVALID_CONFIG);
        EXPECT_EQ(service, nullptr);
    }
}
TEST(SzxMetricsService, LifecycleSnapshotAndBoundedHistory)
{
    szx_metrics_service *service = nullptr;
    ASSERT_EQ(szx_metrics_service_create(nullptr, &service), SZX_METRICS_OK);
    ASSERT_NE(service, nullptr);

    szx_metrics_snapshot *snapshot = nullptr;
    EXPECT_EQ(szx_metrics_service_snapshot(service, &snapshot), SZX_METRICS_NOT_RUNNING);
    EXPECT_EQ(snapshot, nullptr);
    EXPECT_EQ(szx_metrics_service_is_running(service), 0);

    ASSERT_EQ(szx_metrics_service_start(service), SZX_METRICS_OK);
    EXPECT_EQ(szx_metrics_service_start(service), SZX_METRICS_INVALID_CONFIG);
    EXPECT_EQ(szx_metrics_service_is_running(service), 1);

    ASSERT_EQ(szx_metrics_service_snapshot(service, &snapshot), SZX_METRICS_OK);
    ASSERT_NE(snapshot, nullptr);
    EXPECT_EQ(szx_metrics_snapshot_sequence(snapshot), 1U);
    EXPECT_NE(std::string(szx_metrics_snapshot_platform(snapshot)), "unknown");
    EXPECT_FALSE(std::string(szx_metrics_snapshot_instance_id(snapshot)).empty());
    szx_metrics_metric cpu    = sized<szx_metrics_metric>();
    szx_metrics_metric memory = sized<szx_metrics_metric>();
    EXPECT_EQ(szx_metrics_snapshot_cpu(snapshot, &cpu), SZX_METRICS_OK);
    EXPECT_EQ(szx_metrics_snapshot_memory(snapshot, &memory), SZX_METRICS_OK);
    EXPECT_EQ(cpu.state, SZX_METRICS_METRIC_WARMING_UP);
    EXPECT_EQ(memory.state, SZX_METRICS_METRIC_OK);
    EXPECT_LE(szx_metrics_snapshot_disk_count(snapshot), SZX_METRICS_MAX_DISK_PATHS);
    const std::string instance = szx_metrics_snapshot_instance_id(snapshot);
    szx_metrics_snapshot_release(snapshot);

    for (const std::uint32_t window : {0U, 901U})
    {
        szx_metrics_history *history = nullptr;
        EXPECT_EQ(szx_metrics_service_history(service, window, &history), SZX_METRICS_OUT_OF_RANGE);
        EXPECT_EQ(history, nullptr);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2150));
    szx_metrics_history *history = nullptr;
    ASSERT_EQ(szx_metrics_service_history(service, 300, &history), SZX_METRICS_OK);
    ASSERT_NE(history, nullptr);
    EXPECT_EQ(std::string(szx_metrics_history_instance_id(history)), instance);
    EXPECT_GE(szx_metrics_history_count(history), 2U);
    szx_metrics_sample sample = sized<szx_metrics_sample>();
    EXPECT_EQ(szx_metrics_history_sample_at(history, 0, &sample), SZX_METRICS_OK);
    EXPECT_GE(sample.sequence, 1U);
    EXPECT_EQ(szx_metrics_history_sample_at(history, 999999, &sample), SZX_METRICS_OUT_OF_RANGE);
    szx_metrics_history_release(history);

    szx_metrics_service_stop(service);
    EXPECT_EQ(szx_metrics_service_is_running(service), 0);
    EXPECT_EQ(szx_metrics_service_snapshot(service, &snapshot), SZX_METRICS_NOT_RUNNING);
    ASSERT_EQ(szx_metrics_service_start(service), SZX_METRICS_OK);
    ASSERT_EQ(szx_metrics_service_snapshot(service, &snapshot), SZX_METRICS_OK);
    EXPECT_NE(std::string(szx_metrics_snapshot_instance_id(snapshot)), instance);
    szx_metrics_snapshot_release(snapshot);
    szx_metrics_service_destroy(service);
}
TEST(SzxMetricsService, StructSizeCompatibilityTruncatesSafely)
{
    szx_metrics_service *service = nullptr;
    ASSERT_EQ(szx_metrics_service_create(nullptr, &service), SZX_METRICS_OK);
    ASSERT_EQ(szx_metrics_service_start(service), SZX_METRICS_OK);
    szx_metrics_snapshot *snapshot = nullptr;
    ASSERT_EQ(szx_metrics_service_snapshot(service, &snapshot), SZX_METRICS_OK);

    // 旧客户端只认识 v1 前缀：库应只写它声明的大小，不越界
    szx_metrics_metric metric    = sized<szx_metrics_metric>();
    const std::uint32_t declared = 32;
    metric.struct_size           = declared;
    EXPECT_EQ(szx_metrics_snapshot_cpu(snapshot, &metric), SZX_METRICS_OK);
    EXPECT_EQ(metric.struct_size, declared);
    szx_metrics_metric tooSmall{};
    tooSmall.struct_size = 4;
    EXPECT_EQ(szx_metrics_snapshot_cpu(snapshot, &tooSmall), SZX_METRICS_INVALID_ARGUMENT);

    szx_metrics_snapshot_release(snapshot);
    szx_metrics_service_destroy(service);
}
TEST(SzxMetricsService, CardsToggleAndPersistence)
{
    const std::filesystem::path nativePath{u8"szx_metrics_cards_中文_test.txt"};
    const auto utf8Path = nativePath.u8string();
    const std::string statePath(utf8Path.begin(), utf8Path.end());
    std::filesystem::remove(nativePath);
    szx_metrics_config config;
    szx_metrics_config_init(&config);
    config.state_path            = statePath.c_str();
    szx_metrics_service *service = nullptr;
    ASSERT_EQ(szx_metrics_service_create(&config, &service), SZX_METRICS_OK);
    EXPECT_EQ(szx_metrics_service_cards(service), SZX_METRICS_CARD_ALL);
    ASSERT_EQ(szx_metrics_service_set_cards(service, SZX_METRICS_CARD_CPU | SZX_METRICS_CARD_MEMORY), SZX_METRICS_OK);
    EXPECT_EQ(szx_metrics_service_cards(service), SZX_METRICS_CARD_CPU | SZX_METRICS_CARD_MEMORY);
    szx_metrics_service_destroy(service);

    szx_metrics_service *restored = nullptr;
    ASSERT_EQ(szx_metrics_service_create(&config, &restored), SZX_METRICS_OK);
    ASSERT_EQ(szx_metrics_service_start(restored), SZX_METRICS_OK);
    EXPECT_EQ(szx_metrics_service_cards(restored), SZX_METRICS_CARD_CPU | SZX_METRICS_CARD_MEMORY);
    szx_metrics_service_destroy(restored);
    std::filesystem::remove(nativePath);
}
