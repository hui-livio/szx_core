/**
 * @file FeatureManager_test_support.h
 * @brief FeatureManager_test_support.h 覆盖的单元测试与测试辅助。
 */

#pragma once

#include "CompositeFeatureProvider.h"
#include "FeatureManager.h"
#include "LocalFileFeatureProvider.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
    #include <process.h>
#else
    #include <unistd.h>
#endif

namespace
{
using namespace SZX::FEATURE;
using namespace std::chrono_literals;

class ScopedPathCleanup
{
public:
    explicit ScopedPathCleanup(std::filesystem::path path) :
        path_(std::move(path))
    {
    }

    ~ScopedPathCleanup()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

private:
    std::filesystem::path path_;
};

class MemoryProvider : public IFeatureProvider
{
public:
    FeatureLoadResult load() override
    {
        if (failLoad)
            return FeatureLoadResult::Failure(10, "load failed");
        return FeatureLoadResult::Success(values);
    }

    FeaturePersistResult save(const FeatureMap &features) override
    {
        ++saveCalls;
        if (failSave)
            return FeaturePersistResult::Failure(11, "save failed");
        values = features;
        return FeaturePersistResult::Success(values);
    }

    FeatureProviderStatus status() const override
    {
        return providerStatus;
    }

    FeatureMap values;
    FeatureProviderStatus providerStatus;
    bool failLoad = false;
    bool failSave = false;
    int saveCalls = 0;
};

std::filesystem::path tempDirectory()
{
    static std::atomic<unsigned long long> sequence{0};
    // gtest_discover_tests 将每个用例注册为独立 ctest test；ctest 并行（-j）时同一
    // 可执行文件会以多进程运行。仅用进程内递增序号会令各进程都从 0 开始，产生相同
    // 目录名导致互相覆盖/删除（并行 flaky）。附加进程 ID 保证跨进程唯一。
    const unsigned long long pid = []
    {
#ifdef _WIN32
        return static_cast<unsigned long long>(_getpid());
#else
        return static_cast<unsigned long long>(::getpid());
#endif
    }();
    const auto path = std::filesystem::temp_directory_path() /
        ("szx_feature_manager_test_" + std::to_string(pid) + "_" + std::to_string(sequence.fetch_add(1)));
    std::filesystem::create_directories(path);
    return path;
}

void writeText(const std::filesystem::path &path, const std::string &content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}

std::string readText(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class LocalFileFeatureProviderTest : public testing::TestWithParam<LocalFileFeatureProvider::Format>
{
protected:
    void SetUp() override
    {
        directory = tempDirectory();
    }
    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    std::filesystem::path filePath() const
    {
        return directory / (GetParam() == LocalFileFeatureProvider::Format::Json ? "features.json" : "features.yaml");
    }

    std::filesystem::path directory;
};

} // namespace
