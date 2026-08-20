/**
 * @file WorkQueueService.hpp
 * @brief 通用多生产-单消费者工作队列服务，支持多生产者 submit 和后台消费函数执行。
 */

#pragma once

#include "Result.hpp"
#include "UUID.hpp"

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace SZX::COMMON
{

enum class WorkItemState
{
    Created = 0,
    Queued,
    Running,
    Succeeded,
    Failed,
    Cancelled,
};

enum class WorkQueueError
{
    InvalidRequest = -1,
    NotFound       = -2,
    QueueClosed    = -3,
    QueueFull      = -4,
    NotReady       = -5,
    Cancelled      = -6,
};

template <typename Task>
struct WorkSubmitResult
{
    std::string workId;
    std::string requestId;
    WorkItemState state = WorkItemState::Created;
    Task task;
};

template <typename Task, typename ResultValue>
struct WorkSubmitFutureResult : WorkSubmitResult<Task>
{
    std::shared_future<Result<ResultValue>> future;
};

template <typename Task>
struct WorkItemStatus
{
    std::string workId;
    std::string requestId;
    WorkItemState state  = WorkItemState::Created;
    bool cancelRequested = false;
    int error            = 0;
    std::string message;
    Task task;
};

template <typename Task, typename ResultValue>
struct WorkExecutionContext
{
    std::function<bool()> isCancelRequested;
};

template <typename Task, typename ResultValue>
using WorkConsumeFn =
    std::function<Result<ResultValue>(const WorkSubmitResult<Task> &, const WorkExecutionContext<Task, ResultValue> &)>;

struct WorkQueueOptions
{
    std::string workerName;
    std::size_t maxQueuedItems   = 0; // 0 means unlimited.
    std::size_t maxRetainedItems = 0; // 0 means unlimited.
    bool autoStart               = true;
};

template <typename Task, typename ResultValue>
class WorkQueueService
{
    static_assert(std::copy_constructible<Task>, "WorkQueueService requires Task to be copy constructible.");
    static_assert(std::default_initializable<Task>,
                  "WorkQueueService requires Task to be default initializable for Result<T>::Failure.");
    static_assert(std::copy_constructible<ResultValue>, "WorkQueueService requires ResultValue to be copy constructible.");
    static_assert(std::default_initializable<ResultValue>,
                  "WorkQueueService requires ResultValue to be default initializable for Result<T>::Failure.");

public:
    WorkQueueService(WorkQueueOptions options, WorkConsumeFn<Task, ResultValue> consume) :
        state_(std::make_shared<State>(std::move(options), std::move(consume)))
    {
    }

    WorkQueueService(const WorkQueueService &)            = delete;
    WorkQueueService &operator=(const WorkQueueService &) = delete;
    WorkQueueService(WorkQueueService &&)                 = delete;
    WorkQueueService &operator=(WorkQueueService &&)      = delete;

    ~WorkQueueService()
    {
        if (detachCurrentWorkerForDestruction())
            return;
        stop();
    }

    auto start() -> Result<void>
    {
        auto state = state_;
        if (!state->consume)
            return Result<void>::Failure(static_cast<int>(WorkQueueError::InvalidRequest),
                                         makeMessage(*state, "work consume function is empty."));

        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->closed)
            return Result<void>::Failure(static_cast<int>(WorkQueueError::QueueClosed),
                                         makeMessage(*state, "work queue is closed."));
        if (state->running)
            return Result<void>::Success();

        startWorkerLocked();
        return Result<void>::Success();
    }

    auto stop() -> Result<void>
    {
        auto state = state_;
        std::thread workerToJoin;
        const auto currentThreadId = std::this_thread::get_id();
        bool isCurrentWorker       = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            isCurrentWorker = state->workerThreadId == currentThreadId;
        }

        if (isCurrentWorker)
        {
            return Result<void>::Failure(static_cast<int>(WorkQueueError::InvalidRequest),
                                         makeMessage(*state, "work queue cannot be stopped from worker thread."));
        }

        std::unique_lock<std::mutex> stopLock(stopMutex_);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->closed        = true;
            state->stopRequested = true;
            cancelQueuedItemsLocked();
            state->cv.notify_all();
            if (worker_.joinable())
                workerToJoin = std::move(worker_);
        }

        if (workerToJoin.joinable())
        {
            workerToJoin.join();
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->running = false;
        }
        return Result<void>::Success();
    }

    auto submit(Task task) -> Result<WorkSubmitFutureResult<Task, ResultValue>>
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->closed)
            return Result<WorkSubmitFutureResult<Task, ResultValue>>::Failure(static_cast<int>(WorkQueueError::QueueClosed),
                                                                              makeMessage(*state, "work queue is closed."));
        if (state->options.maxQueuedItems > 0 && state->queuedIds.size() >= state->options.maxQueuedItems)
            return Result<WorkSubmitFutureResult<Task, ResultValue>>::Failure(static_cast<int>(WorkQueueError::QueueFull),
                                                                              makeMessage(*state, "work queue is full."));
        if (state->options.autoStart && !state->running)
        {
            if (!state->consume)
                return Result<WorkSubmitFutureResult<Task, ResultValue>>::Failure(
                    static_cast<int>(WorkQueueError::InvalidRequest), makeMessage(*state, "work consume function is empty."));
            startWorkerLocked();
        }

        WorkRecord record;
        record.promise          = std::make_shared<std::promise<Result<ResultValue>>>();
        record.submit.workId    = Uuid::v7().toString();
        record.submit.requestId = Uuid::v4().toString();
        record.submit.state     = WorkItemState::Queued;
        record.submit.task      = std::move(task);
        record.status.workId    = record.submit.workId;
        record.status.requestId = record.submit.requestId;
        record.status.state     = WorkItemState::Queued;
        record.status.task      = record.submit.task;

        WorkSubmitFutureResult<Task, ResultValue> submitted;
        submitted.workId    = record.submit.workId;
        submitted.requestId = record.submit.requestId;
        submitted.state     = record.submit.state;
        submitted.task      = record.submit.task;
        submitted.future    = record.promise->get_future().share();
        state->records.emplace(submitted.workId, std::move(record));
        state->queuedIds.push_back(submitted.workId);
        state->cv.notify_one();
        return Result<WorkSubmitFutureResult<Task, ResultValue>>::Success(std::move(submitted));
    }

    auto cancel(const std::string &workId) -> Result<void>
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        auto found = state->records.find(workId);
        if (found == state->records.end())
            return Result<void>::Failure(static_cast<int>(WorkQueueError::NotFound),
                                         makeMessage(*state, "work item is not found."));

        auto &record = found->second;
        if (record.status.state == WorkItemState::Queued)
        {
            record.status.state = WorkItemState::Cancelled;
            record.submit.state = WorkItemState::Cancelled;
            eraseQueuedIdLocked(workId);
            completePromise(record,
                            Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::Cancelled),
                                                         makeMessage(*state, "work item was cancelled.")));
            recordFinishedLocked(*state, workId, record);
            return Result<void>::Success();
        }
        if (record.status.state == WorkItemState::Running)
        {
            record.status.cancelRequested = true;
            return Result<void>::Success();
        }
        return Result<void>::Failure(static_cast<int>(WorkQueueError::InvalidRequest),
                                     makeMessage(*state, "work item is already finished."));
    }

    auto status(const std::string &workId) const -> Result<WorkItemStatus<Task>>
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        auto found = state->records.find(workId);
        if (found == state->records.end())
            return Result<WorkItemStatus<Task>>::Failure(static_cast<int>(WorkQueueError::NotFound),
                                                         makeMessage(*state, "work item is not found."));
        return Result<WorkItemStatus<Task>>::Success(found->second.status);
    }

    auto result(const std::string &workId) const -> Result<ResultValue>
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        auto found = state->records.find(workId);
        if (found == state->records.end())
            return Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::NotFound),
                                                makeMessage(*state, "work item is not found."));
        if (found->second.status.state != WorkItemState::Succeeded || !found->second.result)
            return Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::NotReady),
                                                makeMessage(*state, "work result is not ready."));
        return Result<ResultValue>::Success(*found->second.result);
    }

    auto remove(const std::string &workId) -> Result<void>
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        auto found = state->records.find(workId);
        if (found == state->records.end())
            return Result<void>::Failure(static_cast<int>(WorkQueueError::NotFound),
                                         makeMessage(*state, "work item is not found."));
        if (found->second.status.state == WorkItemState::Running || found->second.status.state == WorkItemState::Queued)
            return Result<void>::Failure(static_cast<int>(WorkQueueError::InvalidRequest),
                                         makeMessage(*state, "work item is not finished."));
        state->records.erase(found);
        eraseFinishedIdLocked(workId);
        return Result<void>::Success();
    }

    auto clearFinished() -> std::size_t
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        std::size_t removed = 0;
        for (auto iter = state->records.begin(); iter != state->records.end();)
        {
            if (isTerminalState(iter->second.status.state))
            {
                iter = state->records.erase(iter);
                ++removed;
            }
            else
            {
                ++iter;
            }
        }
        state->finishedIds.clear();
        return removed;
    }

    auto queuedSize() const -> std::size_t
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->queuedIds.size();
    }

    /// 在途任务数：排队中（Queued）与运行中（Running）的任务总数。
    /// 用于容量门控：这些任务最终都会完成，属于「已受理但尚未终态」的占用。
    auto inflightSize() const -> std::size_t
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        std::size_t count = state->queuedIds.size();
        for (const auto &[_, record] : state->records)
        {
            if (record.status.state == WorkItemState::Running)
                ++count;
        }
        return count;
    }

    auto retainedSize() const -> std::size_t
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->records.size();
    }

    auto workerName() const -> std::string
    {
        auto state = state_;
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->options.workerName;
    }

private:
    struct WorkRecord
    {
        WorkSubmitResult<Task> submit;
        WorkItemStatus<Task> status;
        std::optional<ResultValue> result;
        std::shared_ptr<std::promise<Result<ResultValue>>> promise;
        bool finishedRecorded = false;
        bool promiseCompleted = false;
    };

    struct State
    {
        State(WorkQueueOptions optionsValue, WorkConsumeFn<Task, ResultValue> consumeValue) :
            options(std::move(optionsValue)),
            consume(std::move(consumeValue))
        {
        }

        WorkQueueOptions options;
        WorkConsumeFn<Task, ResultValue> consume;
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::string> queuedIds;
        std::deque<std::string> finishedIds;
        std::unordered_map<std::string, WorkRecord> records;
        std::thread::id workerThreadId;
        bool running       = false;
        bool closed        = false;
        bool stopRequested = false;
    };

    static auto isTerminalState(WorkItemState state) -> bool
    {
        return state == WorkItemState::Succeeded || state == WorkItemState::Failed || state == WorkItemState::Cancelled;
    }

    auto detachCurrentWorkerForDestruction() -> bool
    {
        auto state                 = state_;
        const auto currentThreadId = std::this_thread::get_id();
        bool isCurrentWorkerThread = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            isCurrentWorkerThread = state->workerThreadId == currentThreadId;
            if (!isCurrentWorkerThread)
                return false;

            state->closed        = true;
            state->stopRequested = true;
            cancelQueuedItemsLocked();
            state->cv.notify_all();
        }

        if (worker_.joinable())
            worker_.detach();
        return true;
    }

    void startWorkerLocked()
    {
        auto state            = state_;
        state->stopRequested  = false;
        state->running        = true;
        worker_               = std::thread([state] { workerLoop(std::move(state)); });
        state->workerThreadId = worker_.get_id();
    }

    static void workerLoop(std::shared_ptr<State> state)
    {
        while (true)
        {
            std::optional<WorkSubmitResult<Task>> job;
            {
                std::unique_lock<std::mutex> lock(state->mutex);
                state->cv.wait(lock, [&state] { return state->stopRequested || !state->queuedIds.empty(); });
                if (state->queuedIds.empty())
                {
                    if (state->stopRequested)
                        break;
                    continue;
                }

                const auto workId = std::move(state->queuedIds.front());
                state->queuedIds.pop_front();
                auto found = state->records.find(workId);
                if (found == state->records.end() || found->second.status.state == WorkItemState::Cancelled)
                    continue;

                found->second.status.state = WorkItemState::Running;
                found->second.submit.state = WorkItemState::Running;
                job                        = found->second.submit;
            }

            auto consumed = consumeJob(state, *job);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                auto found = state->records.find(job->workId);
                if (found == state->records.end())
                    continue;
                if (found->second.status.cancelRequested)
                {
                    found->second.status.state   = WorkItemState::Cancelled;
                    found->second.status.error   = 0;
                    found->second.status.message = makeMessage(*state, "work item was cancelled.");
                    found->second.submit.state   = WorkItemState::Cancelled;
                    found->second.result.reset();
                    completePromise(
                        found->second,
                        Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::Cancelled), found->second.status.message));
                    recordFinishedLocked(*state, job->workId, found->second);
                    continue;
                }
                if (consumed.ok)
                {
                    found->second.result       = std::move(consumed.value);
                    found->second.status.state = WorkItemState::Succeeded;
                    found->second.status.error = 0;
                    found->second.status.message.clear();
                    found->second.submit.state = WorkItemState::Succeeded;
                    completePromise(found->second, Result<ResultValue>::Success(*found->second.result));
                    recordFinishedLocked(*state, job->workId, found->second);
                }
                else
                {
                    found->second.status.state   = WorkItemState::Failed;
                    found->second.status.error   = consumed.error;
                    found->second.status.message = consumed.message;
                    found->second.submit.state   = WorkItemState::Failed;
                    completePromise(found->second, Result<ResultValue>::Failure(consumed.error, consumed.message));
                    recordFinishedLocked(*state, job->workId, found->second);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->running        = false;
            state->workerThreadId = {};
        }
    }

    static auto consumeJob(const std::shared_ptr<State> &state, const WorkSubmitResult<Task> &job) -> Result<ResultValue>
    {
        try
        {
            WorkExecutionContext<Task, ResultValue> context;
            context.isCancelRequested = [state, workId = job.workId]
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                auto found = state->records.find(workId);
                return found != state->records.end() && found->second.status.cancelRequested;
            };
            return state->consume(job, context);
        } catch (const std::exception &error)
        {
            return Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::InvalidRequest),
                                                makeMessage(*state, error.what()));
        } catch (...)
        {
            return Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::InvalidRequest),
                                                makeMessage(*state, "work consume function threw an unknown exception."));
        }
    }

    void cancelQueuedItemsLocked()
    {
        auto state = state_;
        for (const auto &workId : state->queuedIds)
        {
            auto found = state->records.find(workId);
            if (found == state->records.end())
                continue;
            found->second.status.state = WorkItemState::Cancelled;
            found->second.submit.state = WorkItemState::Cancelled;
            completePromise(found->second,
                            Result<ResultValue>::Failure(static_cast<int>(WorkQueueError::Cancelled),
                                                         makeMessage(*state, "work item was cancelled.")));
            recordFinishedLocked(*state, workId, found->second);
        }
        state->queuedIds.clear();
    }

    void eraseQueuedIdLocked(const std::string &workId)
    {
        auto state = state_;
        for (auto iter = state->queuedIds.begin(); iter != state->queuedIds.end(); ++iter)
        {
            if (*iter == workId)
            {
                state->queuedIds.erase(iter);
                return;
            }
        }
    }

    static void recordFinishedLocked(State &state, const std::string &workId, WorkRecord &record)
    {
        if (!record.finishedRecorded)
        {
            state.finishedIds.push_back(workId);
            record.finishedRecorded = true;
        }
        trimRetainedLocked(state);
    }

    static void completePromise(WorkRecord &record, Result<ResultValue> result)
    {
        if (!record.promise || record.promiseCompleted)
            return;
        record.promiseCompleted = true;
        record.promise->set_value(std::move(result));
    }

    static void trimRetainedLocked(State &state)
    {
        while (state.options.maxRetainedItems > 0 && state.finishedIds.size() > state.options.maxRetainedItems)
        {
            const auto workId = state.finishedIds.front();
            state.finishedIds.pop_front();
            auto found = state.records.find(workId);
            if (found != state.records.end() && isTerminalState(found->second.status.state))
                state.records.erase(found);
        }
    }

    void eraseFinishedIdLocked(const std::string &workId)
    {
        auto state = state_;
        for (auto iter = state->finishedIds.begin(); iter != state->finishedIds.end(); ++iter)
        {
            if (*iter == workId)
            {
                state->finishedIds.erase(iter);
                return;
            }
        }
    }

    static auto makeMessage(const State &state, const std::string &message) -> std::string
    {
        if (state.options.workerName.empty())
            return message;
        std::ostringstream stream;
        stream << "[" << state.options.workerName << "] " << message;
        return stream.str();
    }

    std::shared_ptr<State> state_;
    std::thread worker_;
    std::mutex stopMutex_;
};

} // namespace SZX::COMMON
