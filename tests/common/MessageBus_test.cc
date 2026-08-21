/**
 * @file MessageBus_test.cc
 * @brief MessageBus_test.cc 覆盖的单元测试与测试辅助。
 */

#include "MessageBus.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace SZX::COMMON;

class CounterLatch
{
public:
    explicit CounterLatch(int expected) :
        expected_(expected)
    {
    }

    void Increment()
    {
        const auto value = count_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (value >= expected_)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cv_.notify_all();
        }
    }

    bool WaitFor(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return count_.load(std::memory_order_relaxed) >= expected_; });
    }

    int Count() const
    {
        return count_.load(std::memory_order_relaxed);
    }

private:
    int expected_;
    std::atomic<int> count_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};

TEST(common_MessageBusTest, PublishesExactTopicAcrossMultipleProducerThreads)
{
    constexpr int kProducers           = 4;
    constexpr int kMessagesPerProducer = 50;
    constexpr int kExpected            = kProducers * kMessagesPerProducer;

    MessageBus bus(128, 4, FullPolicy::Block);
    CounterLatch latch(kExpected);
    std::atomic<int> sum{0};
    std::atomic<bool> allPublishesSucceeded{true};

    bus.subscribe<int>("pcb.detect.result",
                       [&](const int &value)
                       {
                           sum.fetch_add(value, std::memory_order_relaxed);
                           latch.Increment();
                       });
    bus.start();

    std::vector<std::thread> producers;
    for (int producer = 0; producer < kProducers; ++producer)
    {
        producers.emplace_back(
            [&bus, &allPublishesSucceeded]
            {
                for (int i = 0; i < kMessagesPerProducer; ++i)
                {
                    if (!bus.publish("pcb.detect.result", 1))
                    {
                        allPublishesSucceeded.store(false, std::memory_order_relaxed);
                    }
                }
            });
    }

    for (auto &producer : producers)
    {
        producer.join();
    }

    EXPECT_TRUE(latch.WaitFor(std::chrono::seconds(2)));
    bus.stop();

    EXPECT_TRUE(allPublishesSucceeded.load(std::memory_order_relaxed));
    EXPECT_EQ(latch.Count(), kExpected);
    EXPECT_EQ(sum.load(std::memory_order_relaxed), kExpected);
}

TEST(common_MessageBusTest, WildcardSubscribersReceiveMatchingTopics)
{
    MessageBus bus(64, 2, FullPolicy::Block);
    CounterLatch latch(3);
    std::mutex mutex;
    std::vector<std::string> received;

    bus.subscribe<std::string>("pcb/#",
                               [&](const std::string &value)
                               {
                                   {
                                       std::lock_guard<std::mutex> lock(mutex);
                                       received.push_back(value);
                                   }
                                   latch.Increment();
                               });
    bus.start();

    EXPECT_TRUE(bus.publish("pcb/template/created", std::string("template")));
    EXPECT_TRUE(bus.publish("pcb/detect/result", std::string("result")));
    EXPECT_TRUE(bus.publish("pcb", std::string("root")));
    EXPECT_TRUE(bus.publish("other/topic", std::string("ignored")));

    EXPECT_TRUE(latch.WaitFor(std::chrono::seconds(2)));
    bus.stop();

    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(received.size(), 3U);
    EXPECT_EQ((std::multiset<std::string>(received.begin(), received.end())),
              (std::multiset<std::string>{"result", "root", "template"}));
}

TEST(common_MessageBusTest, UnsubscribeStopsFurtherDelivery)
{
    MessageBus bus(16, 1, FullPolicy::Block);
    CounterLatch firstMessage(1);
    CounterLatch queueDrained(1);
    std::atomic<int> count{0};

    const auto subId = bus.subscribe<int>("pcb.once",
                                          [&](const int &)
                                          {
                                              count.fetch_add(1, std::memory_order_relaxed);
                                              firstMessage.Increment();
                                          });
    bus.subscribe<int>("pcb.barrier", [&](const int &) { queueDrained.Increment(); });
    bus.start();

    EXPECT_TRUE(bus.publish("pcb.once", 1));
    ASSERT_TRUE(firstMessage.WaitFor(std::chrono::seconds(2)));

    bus.unsubscribe(subId);
    EXPECT_TRUE(bus.publish("pcb.once", 2));
    EXPECT_TRUE(bus.publish("pcb.barrier", 0));
    EXPECT_TRUE(queueDrained.WaitFor(std::chrono::seconds(2)));
    bus.stop();

    EXPECT_EQ(count.load(std::memory_order_relaxed), 1);
}

TEST(common_MessageBusTest, ReturnFalsePolicyReportsFullQueue)
{
    MessageBus bus(2, 1, FullPolicy::ReturnFalse);

    EXPECT_TRUE(bus.publish("pcb.queue", 1));
    EXPECT_TRUE(bus.publish("pcb.queue", 2));
    EXPECT_FALSE(bus.publish("pcb.queue", 3));
}

TEST(common_MessageBusTest, DropNewestPolicyKeepsExistingMessagesWhenFull)
{
    MessageBus bus(2, 1, FullPolicy::DropNewest);
    CounterLatch latch(2);
    std::mutex mutex;
    std::vector<int> received;

    bus.subscribe<int>("pcb.queue",
                       [&](const int &value)
                       {
                           {
                               std::lock_guard<std::mutex> lock(mutex);
                               received.push_back(value);
                           }
                           latch.Increment();
                       });

    EXPECT_TRUE(bus.publish("pcb.queue", 1));
    EXPECT_TRUE(bus.publish("pcb.queue", 2));
    EXPECT_TRUE(bus.publish("pcb.queue", 3));

    bus.start();
    EXPECT_TRUE(latch.WaitFor(std::chrono::seconds(2)));
    bus.stop();

    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ((std::multiset<int>(received.begin(), received.end())), (std::multiset<int>{1, 2}));
}

TEST(common_MessageBusTest, DropOldestPolicyDiscardsOldestMessageWhenFull)
{
    MessageBus bus(2, 1, FullPolicy::DropOldest);
    CounterLatch latch(2);
    std::mutex mutex;
    std::vector<int> received;

    bus.subscribe<int>("pcb.queue",
                       [&](const int &value)
                       {
                           {
                               std::lock_guard<std::mutex> lock(mutex);
                               received.push_back(value);
                           }
                           latch.Increment();
                       });

    EXPECT_TRUE(bus.publish("pcb.queue", 1));
    EXPECT_TRUE(bus.publish("pcb.queue", 2));
    EXPECT_TRUE(bus.publish("pcb.queue", 3));

    bus.start();
    EXPECT_TRUE(latch.WaitFor(std::chrono::seconds(2)));
    bus.stop();

    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ((std::multiset<int>(received.begin(), received.end())), (std::multiset<int>{2, 3}));
}
} // namespace
