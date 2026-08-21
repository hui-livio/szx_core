/**
 * @file WorkQueueService_test_support.h
 * @brief WorkQueueService_test_support.h 覆盖的单元测试与测试辅助。
 */

#pragma once

#include "WorkQueueService.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace SZX::COMMON;

struct TestTask
{
    int value = 0;
};

auto waitUntil(const std::function<bool()> &predicate) -> bool
{
    const auto deadline  = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    const auto spinUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
    while (std::chrono::steady_clock::now() < spinUntil)
    {
        if (predicate())
            return true;
        std::this_thread::yield();
    }
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}


} // namespace
