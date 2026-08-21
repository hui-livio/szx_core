/**
 * @file WorkQueueService_concurrency_tests.cc
 * @brief WorkQueueService_concurrency_tests.cc 覆盖的单元测试与测试辅助。
 */

#include "WorkQueueService_test_support.h"

namespace
{
TEST(common_WorkQueueServiceTest, MaxQueuedItemsRejectsExtraSubmit)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "bounded", .maxQueuedItems = 1, .autoStart = false},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto first = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(first.ok) << first.message;

    auto second = service.submit(TestTask{.value = 2});
    EXPECT_FALSE(second.ok);
    EXPECT_EQ(second.error, static_cast<int>(WorkQueueError::QueueFull));
}

TEST(common_WorkQueueServiceTest, MultipleProducersAreConsumedBySingleWorker)
{
    std::mutex valuesMutex;
    std::vector<int> consumedValues;
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "mpsc"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                            {
                                                std::lock_guard<std::mutex> lock(valuesMutex);
                                                consumedValues.push_back(job.task.value);
                                                return Result<int>::Success(job.task.value);
                                            });

    constexpr int kProducerCount    = 4;
    constexpr int kTasksPerProducer = 10;
    std::vector<WorkSubmitResult<TestTask>> submitted;
    std::mutex submittedMutex;
    std::vector<std::thread> producers;
    for (int producer = 0; producer < kProducerCount; ++producer)
    {
        producers.emplace_back(
            [&, producer]
            {
                for (int i = 0; i < kTasksPerProducer; ++i)
                {
                    auto result = service.submit(TestTask{.value = producer * kTasksPerProducer + i});
                    ASSERT_TRUE(result.ok) << result.message;
                    std::lock_guard<std::mutex> lock(submittedMutex);
                    submitted.push_back(result.value);
                }
            });
    }
    for (auto &producer : producers)
        producer.join();

    ASSERT_TRUE(waitUntil(
        [&]
        {
            std::lock_guard<std::mutex> lock(valuesMutex);
            return consumedValues.size() == kProducerCount * kTasksPerProducer;
        }));

    for (const auto &job : submitted)
    {
        auto status = service.status(job.workId);
        ASSERT_TRUE(status.ok) << status.message;
        EXPECT_EQ(status.value.state, WorkItemState::Succeeded);
    }
}

TEST(common_WorkQueueServiceTest, StopCancelsQueuedItemsAndRejectsNewSubmit)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "stop", .autoStart = false},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto submitted = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    ASSERT_TRUE(service.stop().ok);
    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Cancelled);

    ASSERT_EQ(submitted.value.future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    auto result = submitted.value.future.get();
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(WorkQueueError::Cancelled));
    EXPECT_NE(result.message.find("stop"), std::string::npos);

    auto rejected = service.submit(TestTask{.value = 2});
    EXPECT_FALSE(rejected.ok);
    EXPECT_EQ(rejected.error, static_cast<int>(WorkQueueError::QueueClosed));

    auto restarted = service.start();
    EXPECT_FALSE(restarted.ok);
    EXPECT_EQ(restarted.error, static_cast<int>(WorkQueueError::QueueClosed));
}

TEST(common_WorkQueueServiceTest, StopDoesNotCancelRunningWork)
{
    std::atomic<bool> stopStarted{false};
    std::mutex mutex;
    std::condition_variable cv;
    bool running        = false;
    bool observedCancel = true;

    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "stop_running"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &context)
                                            {
                                                {
                                                    std::lock_guard<std::mutex> lock(mutex);
                                                    running = true;
                                                    cv.notify_one();
                                                }
                                                if (!waitUntil([&] { return stopStarted.load(std::memory_order_acquire); }))
                                                    return Result<int>::Failure(1, "stop was not requested.");
                                                observedCancel = context.isCancelRequested();
                                                return Result<int>::Success(job.task.value);
                                            });

    auto submitted = service.submit(TestTask{.value = 11});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return running; }));
    }

    std::thread stopper(
        [&]
        {
            stopStarted.store(true, std::memory_order_release);
            auto stopped = service.stop();
            EXPECT_TRUE(stopped.ok) << stopped.message;
        });
    stopper.join();

    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Succeeded);
    EXPECT_FALSE(observedCancel);

    auto result = service.result(submitted.value.workId);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.value, 11);
}

TEST(common_WorkQueueServiceTest, ConcurrentExternalStopWaitsForRunningWork)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool running = false;
    bool release = false;
    std::atomic<int> stopStarted{0};
    std::atomic<int> stopReturnedBeforeFinish{0};
    std::atomic<bool> taskFinished{false};

    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "concurrent_stop"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                            {
                                                {
                                                    std::lock_guard<std::mutex> lock(mutex);
                                                    running = true;
                                                    cv.notify_one();
                                                }
                                                std::unique_lock<std::mutex> lock(mutex);
                                                cv.wait(lock, [&] { return release; });
                                                taskFinished.store(true, std::memory_order_release);
                                                return Result<int>::Success(job.task.value);
                                            });

    auto submitted = service.submit(TestTask{.value = 13});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return running; }));
    }

    auto stopWorker = [&]
    {
        stopStarted.fetch_add(1, std::memory_order_acq_rel);
        auto stopped = service.stop();
        EXPECT_TRUE(stopped.ok) << stopped.message;
        if (!taskFinished.load(std::memory_order_acquire))
            stopReturnedBeforeFinish.fetch_add(1, std::memory_order_acq_rel);
    };

    std::thread firstStopper(stopWorker);
    std::thread secondStopper(stopWorker);
    ASSERT_TRUE(waitUntil([&] { return stopStarted.load(std::memory_order_acquire) == 2; }));

    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        cv.notify_one();
    }

    firstStopper.join();
    secondStopper.join();

    EXPECT_EQ(stopReturnedBeforeFinish.load(std::memory_order_acquire), 0);
    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Succeeded);
}

TEST(common_WorkQueueServiceTest, RetentionLimitRemovesOldFinishedRecords)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "retention", .maxRetainedItems = 2},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto first = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(first.ok) << first.message;
    auto second = service.submit(TestTask{.value = 2});
    ASSERT_TRUE(second.ok) << second.message;
    auto third = service.submit(TestTask{.value = 3});
    ASSERT_TRUE(third.ok) << third.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(third.value.workId);
            return status.ok && status.value.state == WorkItemState::Succeeded;
        }));

    EXPECT_EQ(service.retainedSize(), 2U);
    auto firstStatus = service.status(first.value.workId);
    EXPECT_FALSE(firstStatus.ok);
    EXPECT_EQ(firstStatus.error, static_cast<int>(WorkQueueError::NotFound));
    EXPECT_TRUE(service.status(second.value.workId).ok);
    EXPECT_TRUE(service.status(third.value.workId).ok);
}

TEST(common_WorkQueueServiceTest, RemoveAndClearFinishedDropStoredRecords)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "cleanup"},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto first = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(first.ok) << first.message;
    auto second = service.submit(TestTask{.value = 2});
    ASSERT_TRUE(second.ok) << second.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(second.value.workId);
            return status.ok && status.value.state == WorkItemState::Succeeded;
        }));

    auto removed = service.remove(first.value.workId);
    ASSERT_TRUE(removed.ok) << removed.message;
    EXPECT_FALSE(service.status(first.value.workId).ok);

    EXPECT_EQ(service.clearFinished(), 1U);
    EXPECT_FALSE(service.status(second.value.workId).ok);
    EXPECT_EQ(service.retainedSize(), 0U);
}


} // namespace
