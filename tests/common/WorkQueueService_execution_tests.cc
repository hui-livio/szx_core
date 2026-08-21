/**
 * @file WorkQueueService_execution_tests.cc
 * @brief WorkQueueService_execution_tests.cc 覆盖的单元测试与测试辅助。
 */

#include "WorkQueueService_test_support.h"

namespace
{
TEST(common_WorkQueueServiceTest, SubmitCreatesQueuedWorkItem)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "submit_only", .autoStart = false},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto submitted = service.submit(TestTask{.value = 7});

    ASSERT_TRUE(submitted.ok) << submitted.message;
    EXPECT_FALSE(submitted.value.workId.empty());
    EXPECT_FALSE(submitted.value.requestId.empty());
    EXPECT_EQ(submitted.value.state, WorkItemState::Queued);
    EXPECT_EQ(submitted.value.task.value, 7);
    EXPECT_EQ(service.queuedSize(), 1U);

    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Queued);
    EXPECT_EQ(status.value.task.value, 7);
}

TEST(common_WorkQueueServiceTest, WorkerCallsConsumeAndStoresResult)
{
    std::atomic<int> callCount{0};
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "consume_success"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                            {
                                                callCount.fetch_add(1, std::memory_order_relaxed);
                                                return Result<int>::Success(job.task.value * 2);
                                            });

    ASSERT_TRUE(service.start().ok);
    auto submitted = service.submit(TestTask{.value = 21});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(submitted.value.workId);
            return status.ok && status.value.state == WorkItemState::Succeeded;
        }));

    EXPECT_EQ(callCount.load(std::memory_order_relaxed), 1);
    auto result = service.result(submitted.value.workId);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.value, 42);
}

TEST(common_WorkQueueServiceTest, SubmitReturnsFutureThatWaitsForConsumeResult)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "submit_future"},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value * 3); });

    auto submitted = service.submit(TestTask{.value = 14});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    submitted.value.future.wait();
    auto result = submitted.value.future.get();
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.value, 42);
}

TEST(common_WorkQueueServiceTest, SubmitAutoStartsWorkerByDefault)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "auto_start"},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value + 1); });

    auto submitted = service.submit(TestTask{.value = 4});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(submitted.value.workId);
            return status.ok && status.value.state == WorkItemState::Succeeded;
        }));

    auto result = service.result(submitted.value.workId);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.value, 5);
}

TEST(common_WorkQueueServiceTest, FailedConsumeStoresFailureStatus)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "consume_failure"},
                                            [](const WorkSubmitResult<TestTask> &, const auto &)
                                            { return Result<int>::Failure(42, "planned failure"); });

    ASSERT_TRUE(service.start().ok);
    auto submitted = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(submitted.value.workId);
            return status.ok && status.value.state == WorkItemState::Failed;
        }));

    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.error, 42);
    EXPECT_EQ(status.value.message, "planned failure");

    auto result = service.result(submitted.value.workId);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(WorkQueueError::NotReady));

    ASSERT_EQ(submitted.value.future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    auto futureResult = submitted.value.future.get();
    EXPECT_FALSE(futureResult.ok);
    EXPECT_EQ(futureResult.error, 42);
    EXPECT_EQ(futureResult.message, "planned failure");
}

TEST(common_WorkQueueServiceTest, ConsumeExceptionIsCapturedAsFailure)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "consume_throw"},
                                            [](const WorkSubmitResult<TestTask> &, const auto &) -> Result<int>
                                            { throw std::runtime_error("boom"); });

    ASSERT_TRUE(service.start().ok);
    auto submitted = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(submitted.value.workId);
            return status.ok && status.value.state == WorkItemState::Failed;
        }));

    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.error, static_cast<int>(WorkQueueError::InvalidRequest));
    EXPECT_EQ(status.value.message, "[consume_throw] boom");
}

TEST(common_WorkQueueServiceTest, CancelQueuedWorkRemovesFromQueue)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "cancel_queued", .autoStart = false},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto submitted = service.submit(TestTask{.value = 3});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    auto cancelled = service.cancel(submitted.value.workId);
    ASSERT_TRUE(cancelled.ok) << cancelled.message;
    EXPECT_EQ(service.queuedSize(), 0U);

    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Cancelled);

    ASSERT_EQ(submitted.value.future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    auto result = submitted.value.future.get();
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(WorkQueueError::Cancelled));
    EXPECT_NE(result.message.find("cancel_queued"), std::string::npos);
}

TEST(common_WorkQueueServiceTest, CancelRunningWorkMarksCancelRequested)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool running = false;
    bool release = false;

    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "cancel_running"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                            {
                                                {
                                                    std::lock_guard<std::mutex> lock(mutex);
                                                    running = true;
                                                    cv.notify_one();
                                                }
                                                std::unique_lock<std::mutex> lock(mutex);
                                                cv.wait(lock, [&] { return release; });
                                                return Result<int>::Success(job.task.value);
                                            });

    ASSERT_TRUE(service.start().ok);
    auto submitted = service.submit(TestTask{.value = 9});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return running; }));
    }

    auto cancelled = service.cancel(submitted.value.workId);
    ASSERT_TRUE(cancelled.ok) << cancelled.message;
    auto status = service.status(submitted.value.workId);
    ASSERT_TRUE(status.ok) << status.message;
    EXPECT_EQ(status.value.state, WorkItemState::Running);
    EXPECT_TRUE(status.value.cancelRequested);

    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        cv.notify_one();
    }

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto updated = service.status(submitted.value.workId);
            return updated.ok && updated.value.state == WorkItemState::Cancelled;
        }));

    auto result = service.result(submitted.value.workId);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, static_cast<int>(WorkQueueError::NotReady));

    ASSERT_EQ(submitted.value.future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    auto futureResult = submitted.value.future.get();
    EXPECT_FALSE(futureResult.ok);
    EXPECT_EQ(futureResult.error, static_cast<int>(WorkQueueError::Cancelled));
    EXPECT_NE(futureResult.message.find("cancel_running"), std::string::npos);
}

TEST(common_WorkQueueServiceTest, ConsumeFunctionCanObserveCancelRequest)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool running        = false;
    bool release        = false;
    bool observedCancel = false;

    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "cancel_context"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &context)
                                            {
                                                {
                                                    std::lock_guard<std::mutex> lock(mutex);
                                                    running = true;
                                                    cv.notify_one();
                                                }
                                                std::unique_lock<std::mutex> lock(mutex);
                                                cv.wait(lock, [&] { return release; });
                                                observedCancel = context.isCancelRequested();
                                                return Result<int>::Success(job.task.value);
                                            });

    auto submitted = service.submit(TestTask{.value = 8});
    ASSERT_TRUE(submitted.ok) << submitted.message;

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return running; }));
    }

    ASSERT_TRUE(service.cancel(submitted.value.workId).ok);
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        cv.notify_one();
    }

    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(submitted.value.workId);
            return status.ok && status.value.state == WorkItemState::Cancelled;
        }));
    EXPECT_TRUE(observedCancel);
}

TEST(common_WorkQueueServiceTest, InflightSizeCountsQueuedAndRunningWork)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool running = false;
    bool release = false;

    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "inflight"},
                                            [&](const WorkSubmitResult<TestTask> &job, const auto &)
                                            {
                                                {
                                                    std::lock_guard<std::mutex> lock(mutex);
                                                    running = true;
                                                    cv.notify_one();
                                                }
                                                std::unique_lock<std::mutex> lock(mutex);
                                                cv.wait(lock, [&] { return release; });
                                                return Result<int>::Success(job.task.value);
                                            });

    ASSERT_TRUE(service.start().ok);
    auto first = service.submit(TestTask{.value = 1});
    ASSERT_TRUE(first.ok) << first.message;
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return running; }));
    }
    // 第一个任务运行中（Running），第二个排队（Queued）→ inflight = 2。
    auto second = service.submit(TestTask{.value = 2});
    ASSERT_TRUE(second.ok) << second.message;
    EXPECT_EQ(service.inflightSize(), 2U);

    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
        cv.notify_all();
    }
    ASSERT_TRUE(waitUntil(
        [&]
        {
            auto status = service.status(second.value.workId);
            return status.ok && status.value.state == WorkItemState::Succeeded;
        }));
    // 全部完成后 inflight 归零（终态任务不计入）。
    EXPECT_EQ(service.inflightSize(), 0U);
}

TEST(common_WorkQueueServiceTest, InflightSizeExcludesQueuedCancel)
{
    WorkQueueService<TestTask, int> service(WorkQueueOptions{.workerName = "inflight_cancel", .autoStart = false},
                                            [](const WorkSubmitResult<TestTask> &job, const auto &)
                                            { return Result<int>::Success(job.task.value); });

    auto submitted = service.submit(TestTask{.value = 5});
    ASSERT_TRUE(submitted.ok) << submitted.message;
    EXPECT_EQ(service.inflightSize(), 1U);

    ASSERT_TRUE(service.cancel(submitted.value.workId).ok);
    EXPECT_EQ(service.inflightSize(), 0U);
}


} // namespace
