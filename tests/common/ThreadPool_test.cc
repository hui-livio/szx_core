/**
 * @file ThreadPool_test.cc
 */

#include "ThreadPool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace SZX::COMMON;

namespace
{

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

TEST(common_ThreadPoolTest, AutoStartExecutesSubmittedTask)
{
    ThreadPool pool(std::size_t{2});
    EXPECT_TRUE(pool.running());

    auto future = pool.submit([] { return 6 * 7; });
    EXPECT_EQ(future.get(), 42);
}

TEST(common_ThreadPoolTest, ManualStartControlsSubmissionState)
{
    ThreadPool pool(ThreadPoolOptions{.workerCount = 1, .autoStart = false});
    EXPECT_FALSE(pool.running());
    EXPECT_FALSE(pool.post([] {}));
    EXPECT_THROW(pool.submit([] { return 1; }), std::runtime_error);

    pool.start();
    EXPECT_TRUE(pool.running());

    auto future = pool.submit([] { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST(common_ThreadPoolTest, SubmitAndPostExecute)
{
    ThreadPool pool(ThreadPoolOptions{.workerCount = 2});
    std::atomic<int> value{0};
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 10; ++i)
    {
        futures.push_back(pool.submit([&value] { return value.fetch_add(1) + 1; }));
    }

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_GE(futures[i].get(), 1);
    }
    EXPECT_EQ(value.load(), 10);

    std::atomic<int> postCount{0};
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_TRUE(pool.post([&postCount] { postCount.fetch_add(1); }));
    }
    EXPECT_TRUE(waitUntil([&] { return postCount.load() == 5; }));
}

TEST(common_ThreadPoolTest, MultipleProducersSubmitSafely)
{
    ThreadPool pool(ThreadPoolOptions{.workerCount = 4});
    constexpr int kProducerCount    = 4;
    constexpr int kTasksPerProducer = 20;

    std::mutex futuresMutex;
    std::vector<std::future<int>> futures;
    futures.reserve(kProducerCount * kTasksPerProducer);
    std::vector<std::thread> producers;

    for (int producer = 0; producer < kProducerCount; ++producer)
    {
        producers.emplace_back(
            [&pool, &futures, &futuresMutex, producer]
            {
                for (int i = 0; i < kTasksPerProducer; ++i)
                {
                    const int value = producer * kTasksPerProducer + i;
                    auto future     = pool.submit([value] { return value; });
                    std::lock_guard<std::mutex> lock(futuresMutex);
                    futures.push_back(std::move(future));
                }
            });
    }

    for (auto &producer : producers)
    {
        producer.join();
    }

    ASSERT_EQ(futures.size(), kProducerCount * kTasksPerProducer);
    int sum = 0;
    for (auto &future : futures)
    {
        sum += future.get();
    }

    constexpr int kExpectedCount = kProducerCount * kTasksPerProducer;
    EXPECT_EQ(sum, (kExpectedCount - 1) * kExpectedCount / 2);
}

TEST(common_ThreadPoolTest, ShutdownWaitsForRunningJobs)
{
    ThreadPool pool(std::size_t{1});
    std::atomic<bool> canFinish{false};
    std::atomic<bool> running{false};
    std::atomic<bool> shutdownReturned{false};

    auto future = pool.submit(
        [&running, &canFinish]
        {
            running.store(true, std::memory_order_release);
            while (!canFinish.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

    ASSERT_TRUE(waitUntil([&] { return running.load(std::memory_order_acquire); }));

    std::thread stopThread(
        [&]
        {
            pool.shutdown();
            shutdownReturned.store(true, std::memory_order_release);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(shutdownReturned.load(std::memory_order_acquire));

    canFinish.store(true, std::memory_order_release);
    stopThread.join();
    EXPECT_TRUE(shutdownReturned.load(std::memory_order_acquire));
    EXPECT_TRUE(future.valid());
    EXPECT_NO_THROW(future.get());
    EXPECT_FALSE(pool.running());

    EXPECT_FALSE(pool.post([] {}));
    EXPECT_THROW(pool.submit([] { return 1; }), std::runtime_error);
}

TEST(common_ThreadPoolTest, ShutdownDrainsQueuedJobs)
{
    ThreadPool pool(std::size_t{1});
    std::atomic<bool> canFinish{false};
    std::atomic<bool> running{false};
    std::atomic<bool> shutdownReturned{false};

    auto blocker = pool.submit(
        [&running, &canFinish]
        {
            running.store(true, std::memory_order_release);
            while (!canFinish.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return 1;
        });
    auto queuedA = pool.submit([] { return 2; });
    auto queuedB = pool.submit([] { return 3; });

    ASSERT_TRUE(waitUntil([&] { return running.load(std::memory_order_acquire); }));

    std::thread stopThread(
        [&]
        {
            pool.shutdown();
            shutdownReturned.store(true, std::memory_order_release);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(shutdownReturned.load(std::memory_order_acquire));

    canFinish.store(true, std::memory_order_release);
    stopThread.join();

    EXPECT_EQ(blocker.get(), 1);
    EXPECT_EQ(queuedA.get(), 2);
    EXPECT_EQ(queuedB.get(), 3);
    EXPECT_TRUE(shutdownReturned.load(std::memory_order_acquire));
    EXPECT_FALSE(pool.running());
}

TEST(common_ThreadPoolTest, ExceptionTaskDoesNotBreakPool)
{
    ThreadPool pool(std::size_t{1});
    std::atomic<bool> recovered{false};

    auto first  = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    auto second = pool.submit(
        [&recovered]
        {
            recovered.store(true);
            return 7;
        });

    EXPECT_THROW(first.get(), std::runtime_error);
    EXPECT_EQ(second.get(), 7);
    EXPECT_TRUE(recovered.load());
}

TEST(common_ThreadPoolTest, PostExceptionDoesNotBreakPool)
{
    ThreadPool pool(std::size_t{1});
    std::atomic<bool> reached{false};

    EXPECT_TRUE(pool.post([] { throw std::runtime_error("boom"); }));
    auto future = pool.submit(
        [&reached]
        {
            reached.store(true, std::memory_order_release);
            return 9;
        });

    EXPECT_EQ(future.get(), 9);
    EXPECT_TRUE(reached.load(std::memory_order_acquire));
}

TEST(common_ThreadPoolTest, MultiWorkerCanRunTasksInParallel)
{
    ThreadPool pool(ThreadPoolOptions{.workerCount = 4});
    std::atomic<int> inFlight{0};
    std::atomic<bool> go{false};
    std::mutex peakMutex;
    std::condition_variable peakCv;
    int maxInFlight = 0;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 8; ++i)
    {
        futures.push_back(pool.submit(
            [&inFlight, &go, &peakMutex, &peakCv, &maxInFlight]
            {
                const int current = inFlight.fetch_add(1, std::memory_order_acq_rel) + 1;
                {
                    std::lock_guard<std::mutex> lock(peakMutex);
                    if (current > maxInFlight)
                        maxInFlight = current;
                    peakCv.notify_all();
                }
                while (!go.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                inFlight.fetch_sub(1, std::memory_order_acq_rel);
                return current;
            }));
    }

    {
        std::unique_lock<std::mutex> lock(peakMutex);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool reached        = peakCv.wait_until(lock, deadline, [&] { return maxInFlight >= 2; });
        EXPECT_TRUE(reached) << "expected at least 2 concurrent in-flight tasks";
        EXPECT_GE(maxInFlight, 2);
    }
    go.store(true, std::memory_order_release);

    for (auto &future : futures)
    {
        EXPECT_GE(future.get(), 1);
    }
}

TEST(common_ThreadPoolTest, WorkerCountZeroFallsBackToOne)
{
    ThreadPool pool(ThreadPoolOptions{.workerCount = 0});
    EXPECT_TRUE(pool.running());

    auto future = pool.submit([] { return 1; });
    EXPECT_EQ(future.get(), 1);
}

TEST(common_ThreadPoolTest, PartialWorkerStartupFailureJoinsStartedThreads)
{
    std::atomic<int> attempts{0};
    ThreadPool pool(ThreadPoolOptions{
        .workerCount = 3,
        .autoStart   = false,
        .threadFactory =
            [&attempts](std::function<void()> task)
        {
            if (attempts.fetch_add(1) == 1)
                throw std::runtime_error("injected thread creation failure");
            return std::thread(std::move(task));
        },
    });

    EXPECT_THROW(pool.start(), std::runtime_error);
    EXPECT_FALSE(pool.running());
}

} // namespace
