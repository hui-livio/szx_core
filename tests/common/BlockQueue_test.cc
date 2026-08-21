/**
 * @file BlockQueue_test.cc
 * @brief BlockQueue_test.cc 覆盖的单元测试与测试辅助。
 */

#include "BlockQueue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace
{
using namespace SZX::COMMON;

TEST(common_BlockQueueTest, PushAndTryPop)
{
    BlockingQueue<int> queue;

    EXPECT_TRUE(queue.push(3));
    EXPECT_EQ(queue.size(), 1U);

    int value = 0;
    EXPECT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(queue.empty());
}

TEST(common_BlockQueueTest, CloseRejectsNewValuesButDrainsExisting)
{
    BlockingQueue<int> queue;
    EXPECT_TRUE(queue.push(1));

    queue.close();

    EXPECT_TRUE(queue.is_closed());
    EXPECT_FALSE(queue.push(2));

    int value = 0;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_FALSE(queue.pop(value));
}

TEST(common_BlockQueueTest, PopForTimesOutWhenEmpty)
{
    BlockingQueue<int> queue;
    int value = 0;

    EXPECT_FALSE(queue.pop_for(value, std::chrono::milliseconds(1)));
    EXPECT_FALSE(queue.pop_for(std::chrono::milliseconds(1)).has_value());
}

TEST(common_BlockQueueTest, EmplaceAndOptionalPop)
{
    BlockingQueue<std::string> queue;

    EXPECT_TRUE(queue.emplace(3, 'x'));

    auto value = queue.pop();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "xxx");
    EXPECT_TRUE(queue.empty());
}

TEST(common_BlockQueueTest, ClearRemovesQueuedValues)
{
    BlockingQueue<int> queue;
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));

    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0U);

    int value = 0;
    EXPECT_FALSE(queue.try_pop(value));
}

TEST(common_BlockQueueTest, CloseRejectsEmplace)
{
    BlockingQueue<std::string> queue;

    queue.close();

    EXPECT_FALSE(queue.emplace("rejected"));
}

TEST(common_BlockQueueTest, BlockingPopWaitsUntilProducerPushes)
{
    BlockingQueue<int> queue;
    std::mutex mutex;
    std::condition_variable cv;
    bool consumerWaiting = false;
    int popped           = 0;
    std::atomic<bool> popSucceeded{false};

    std::thread consumer(
        [&]
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                consumerWaiting = true;
                cv.notify_one();
            }
            popSucceeded.store(queue.pop(popped), std::memory_order_release);
        });

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return consumerWaiting; }));
    }

    EXPECT_TRUE(queue.push(42));
    consumer.join();

    EXPECT_TRUE(popSucceeded.load(std::memory_order_acquire));
    EXPECT_EQ(popped, 42);
    EXPECT_TRUE(queue.empty());
}

TEST(common_BlockQueueTest, CloseWakesBlockedConsumer)
{
    BlockingQueue<int> queue;
    std::atomic<bool> popReturned{false};
    std::atomic<bool> popResult{true};

    std::thread consumer(
        [&]
        {
            int value = 0;
            popResult.store(queue.pop(value), std::memory_order_relaxed);
            popReturned.store(true, std::memory_order_release);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    queue.close();
    consumer.join();

    EXPECT_TRUE(popReturned.load(std::memory_order_acquire));
    EXPECT_FALSE(popResult.load(std::memory_order_relaxed));
}

TEST(common_BlockQueueTest, MultipleProducersAndConsumersDrainAllValues)
{
    constexpr int kProducers         = 4;
    constexpr int kConsumers         = 3;
    constexpr int kValuesPerProducer = 100;
    constexpr int kTotalValues       = kProducers * kValuesPerProducer;

    BlockingQueue<int> queue;
    std::atomic<int> consumedCount{0};
    std::atomic<bool> allPushesSucceeded{true};
    std::mutex valuesMutex;
    std::unordered_set<int> consumedValues;
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int c = 0; c < kConsumers; ++c)
    {
        consumers.emplace_back(
            [&]
            {
                int value = 0;
                while (queue.pop(value))
                {
                    consumedCount.fetch_add(1, std::memory_order_relaxed);
                    std::lock_guard<std::mutex> lock(valuesMutex);
                    consumedValues.insert(value);
                }
            });
    }

    for (int p = 0; p < kProducers; ++p)
    {
        producers.emplace_back(
            [&, p]
            {
                const int base = p * kValuesPerProducer;
                for (int i = 0; i < kValuesPerProducer; ++i)
                {
                    if (!queue.push(base + i))
                    {
                        allPushesSucceeded.store(false, std::memory_order_relaxed);
                    }
                }
            });
    }

    for (auto &producer : producers)
    {
        producer.join();
    }
    queue.close();

    for (auto &consumer : consumers)
    {
        consumer.join();
    }

    EXPECT_TRUE(allPushesSucceeded.load(std::memory_order_relaxed));
    EXPECT_EQ(consumedCount.load(std::memory_order_relaxed), kTotalValues);
    EXPECT_EQ(consumedValues.size(), static_cast<size_t>(kTotalValues));
    EXPECT_TRUE(queue.empty());
}

TEST(common_BlockQueueTest, BoundedQueueBlocksProducerUntilConsumerFreesCapacity)
{
    BlockingQueue<int> queue(1);
    ASSERT_EQ(queue.capacity(), 1U);
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.full());

    std::atomic<bool> secondPushReturned{false};
    std::atomic<bool> secondPushResult{false};
    std::thread producer(
        [&]
        {
            secondPushResult.store(queue.push(2), std::memory_order_relaxed);
            secondPushReturned.store(true, std::memory_order_release);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(secondPushReturned.load(std::memory_order_acquire));

    int first = 0;
    ASSERT_TRUE(queue.pop(first));
    EXPECT_EQ(first, 1);
    producer.join();
    EXPECT_TRUE(secondPushReturned.load(std::memory_order_acquire));
    EXPECT_TRUE(secondPushResult.load(std::memory_order_relaxed));

    int second = 0;
    ASSERT_TRUE(queue.pop(second));
    EXPECT_EQ(second, 2);
}

TEST(common_BlockQueueTest, ClosingBoundedQueueWakesBlockedProducer)
{
    BlockingQueue<int> queue(1);
    ASSERT_TRUE(queue.push(1));

    std::atomic<bool> pushResult{true};
    std::thread producer([&] { pushResult.store(queue.push(2), std::memory_order_release); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    queue.close();
    producer.join();

    EXPECT_FALSE(pushResult.load(std::memory_order_acquire));
}

TEST(common_BlockQueueTest, TracksHighWaterMarkInsideQueueLock)
{
    BlockingQueue<int> queue(3);
    EXPECT_EQ(queue.high_water_mark(), 0U);

    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    EXPECT_EQ(queue.high_water_mark(), 2U);

    int value = 0;
    ASSERT_TRUE(queue.pop(value));
    ASSERT_TRUE(queue.push(3));
    ASSERT_TRUE(queue.push(4));
    EXPECT_EQ(queue.high_water_mark(), 3U);

    queue.clear();
    EXPECT_EQ(queue.high_water_mark(), 3U);
}
} // namespace
