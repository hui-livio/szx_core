/**
 * @file WorkQueueService_lifecycle_tests.cc
 * @brief WorkQueueService_lifecycle_tests.cc 覆盖的单元测试与测试辅助。
 */

#include "WorkQueueService_test_support.h"

namespace
{
TEST(common_WorkQueueServiceTest, StartRejectsEmptyConsumeFunction)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "empty"}, {});

    auto started = service.start();
    EXPECT_FALSE(started.ok);
    EXPECT_EQ(started.error, static_cast<int>(WorkQueueError::InvalidRequest));
}

TEST(common_WorkQueueServiceTest, StartIsIdempotentWhenAlreadyRunning)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "idempotent_start"},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto first = service.start();
    ASSERT_TRUE(first.ok) << first.message;
    auto second = service.start();
    EXPECT_TRUE(second.ok) << second.message;
}

TEST(common_WorkQueueServiceTest, SubmitRejectsEmptyConsumeFunctionWhenAutoStartIsEnabled)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "empty_submit"}, {});

    auto submitted = service.submit(TestTask{.value = 1});
    EXPECT_FALSE(submitted.ok);
    EXPECT_EQ(submitted.error, static_cast<int>(WorkQueueError::InvalidRequest));
    EXPECT_NE(submitted.message.find("empty_submit"), std::string::npos);
}

TEST(common_WorkQueueServiceTest, StopFromConsumeFunctionIsRejected)
{
    std::atomic<bool> stopWasRejected{false};
    std::unique_ptr<WorkQueueService<TestTask, int>> service;
    service = std::make_unique<WorkQueueService<TestTask, int>>(WorkQueueOptions{.workerName = "self_stop"},
                                                                [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                                                {
                                                                    auto stopped = service->stop();
                                                                    EXPECT_FALSE(stopped.ok);
                                                                    EXPECT_EQ(stopped.error,
                                                                              static_cast<int>(WorkQueueError::InvalidRequest));
                                                                    auto status = service->status(job.workId);
                                                                    EXPECT_TRUE(status.ok) << status.message;
                                                                    EXPECT_EQ(status.value.state, WorkItemState::Running);
                                                                    stopWasRejected.store(true, std::memory_order_release);
                                                                    return Result<int>::Success(job.task.value);
                                                                });

    auto submitted = service->submit(TestTask{.value = 5});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service->status(submitted.value.workId);
            return status.ok && status.value.state == WorkItemState::Succeeded;
        }));

    auto result = service->result(submitted.value.workId);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.value, 5);
    EXPECT_TRUE(stopWasRejected.load(std::memory_order_acquire));

    auto stopped = service->stop();
    EXPECT_TRUE(stopped.ok) << stopped.message;
}

TEST(common_WorkQueueServiceTest, DestroyedFromConsumeFunctionCompletesCurrentAndCancelsQueuedWork)
{
    std::unique_ptr<WorkQueueService<TestTask, int>> service;
    std::atomic<bool> destroyedFromWorker{false};
    std::mutex mutex;
    std::condition_variable cv;
    bool running      = false;
    bool allowDestroy = false;
    service =
        std::make_unique<WorkQueueService<TestTask, int>>(WorkQueueOptions{.workerName = "self_destroy", .autoStart = false},
                                                          [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                                          {
                                                              {
                                                                  std::lock_guard<std::mutex> lock(mutex);
                                                                  running = true;
                                                                  cv.notify_one();
                                                              }
                                                              {
                                                                  std::unique_lock<std::mutex> lock(mutex);
                                                                  cv.wait(lock, [&] { return allowDestroy; });
                                                              }
                                                              service.reset();
                                                              destroyedFromWorker.store(true, std::memory_order_release);
                                                              return Result<int>::Success(job.task.value);
                                                          });

    auto first = service->submit(TestTask{.value = 31});
    ASSERT_TRUE(first.ok) << first.message;
    auto second = service->submit(TestTask{.value = 32});
    ASSERT_TRUE(second.ok) << second.message;

    auto firstFuture  = first.value.future;
    auto secondFuture = second.value.future;

    auto started = service->start();
    ASSERT_TRUE(started.ok) << started.message;
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return running; }));
        allowDestroy = true;
        cv.notify_one();
    }

    ASSERT_EQ(firstFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    auto firstResult = firstFuture.get();
    ASSERT_TRUE(firstResult.ok) << firstResult.message;
    EXPECT_EQ(firstResult.value, 31);

    ASSERT_EQ(secondFuture.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    auto secondResult = secondFuture.get();
    EXPECT_FALSE(secondResult.ok);
    EXPECT_EQ(secondResult.error, static_cast<int>(WorkQueueError::Cancelled));
    EXPECT_TRUE(destroyedFromWorker.load(std::memory_order_acquire));
    EXPECT_EQ(service, nullptr);
}

TEST(common_WorkQueueServiceTest, ExternalStopWaitsAfterWorkerThreadRequestsStop)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool workerStopReturned = false;
    bool release            = false;
    std::atomic<bool> taskFinished{false};
    std::atomic<bool> externalStopReturned{false};
    std::unique_ptr<WorkQueueService<TestTask, int>> service;

    service = std::make_unique<WorkQueueService<TestTask, int>>(WorkQueueOptions{.workerName = "self_then_external_stop"},
                                                                [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                                                {
                                                                    auto stopped = service->stop();
                                                                    EXPECT_FALSE(stopped.ok);
                                                                    EXPECT_EQ(stopped.error,
                                                                              static_cast<int>(WorkQueueError::InvalidRequest));
                                                                    {
                                                                        std::lock_guard<std::mutex> lock(mutex);
                                                                        workerStopReturned = true;
                                                                        cv.notify_one();
                                                                    }

                                                                    std::unique_lock<std::mutex> lock(mutex);
                                                                    cv.wait(lock, [&] { return release; });
                                                                    taskFinished.store(true, std::memory_order_release);
                                                                    return Result<int>::Success(job.task.value);
                                                                });

    auto submitted = service->submit(TestTask{.value = 17});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return workerStopReturned; }));
    }

    std::thread externalStopper(
        [&]
        {
            auto stopped = service->stop();
            EXPECT_TRUE(stopped.ok) << stopped.message;
            EXPECT_TRUE(taskFinished.load(std::memory_order_acquire));
            externalStopReturned.store(true, std::memory_order_release);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(externalStopReturned.load(std::memory_order_acquire));

    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        cv.notify_one();
    }
    externalStopper.join();

    EXPECT_TRUE(externalStopReturned.load(std::memory_order_acquire));
    auto status = service->status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Succeeded);
}


} // namespace
