/**
 * @file ThreadPool.hpp
 * @brief 通用线程池实现，支持多生产者和多消费者（worker）模式。
 */

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace SZX::COMMON
{

struct ThreadPoolOptions
{
    using ThreadFactory = std::function<std::thread(std::function<void()>)>;

    std::string workerName;
    std::size_t workerCount = std::thread::hardware_concurrency();
    bool autoStart          = true;
    ThreadFactory threadFactory;
};

class ThreadPool
{
public:
    explicit ThreadPool(ThreadPoolOptions options = {}) :
        options_(normalizeOptions(std::move(options)))
    {
        if (options_.autoStart)
            start();
    }

    explicit ThreadPool(std::size_t workerCount, std::string workerName = {}) :
        ThreadPool(ThreadPoolOptions{.workerName = std::move(workerName), .workerCount = workerCount, .autoStart = true})
    {
    }

    ~ThreadPool()
    {
        shutdown();
    }

    ThreadPool(const ThreadPool &)            = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&)                 = delete;
    ThreadPool &operator=(ThreadPool &&)      = delete;

    void start()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (running_)
            return;

        stopped_ = false;
        running_ = true;
        workers_.clear();
        try
        {
            workers_.reserve(options_.workerCount);
            for (std::size_t i = 0; i < options_.workerCount; ++i)
            {
                if (options_.threadFactory)
                    workers_.push_back(options_.threadFactory([this] { workerLoop(); }));
                else
                    workers_.emplace_back(&ThreadPool::workerLoop, this);
            }
        } catch (...)
        {
            stopped_ = true;
            lock.unlock();
            cv_.notify_all();
            joinWorkers();
            lock.lock();
            running_ = false;
            throw;
        }
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_)
                return;
            stopped_ = true;
        }
        cv_.notify_all();

        joinWorkers();

        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }

    template <typename F, typename... Args>
    auto submit(F &&func, Args &&...args) -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    {
        using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
        auto task =
            std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F>(func), std::forward<Args>(args)...));

        std::future<ReturnType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_ || stopped_)
            {
                throw std::runtime_error("thread pool is not running.");
            }

            tasks_.emplace_back([task = std::move(task)]() mutable { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    template <typename F>
    bool post(F &&task)
    {
        static_assert(std::is_convertible_v<F, std::function<void()>>,
                      "ThreadPool::post expects callable with void() signature.");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_ || stopped_)
                return false;

            tasks_.emplace_back(std::forward<F>(task));
        }
        cv_.notify_one();
        return true;
    }

    auto running() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_ && !stopped_;
    }

    auto queued() const -> std::size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    void joinWorkers()
    {
        for (auto &worker : workers_)
        {
            if (worker.joinable())
                worker.join();
        }
        workers_.clear();
    }

    static auto normalizeOptions(ThreadPoolOptions options) -> ThreadPoolOptions
    {
        if (options.workerCount == 0)
            options.workerCount = 1;
        return options;
    }

    void workerLoop()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
                if (stopped_ && tasks_.empty())
                    break;

                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            if (!task)
                continue;
            try
            {
                task();
            } catch (...)
            {
                // 任务异常在此兜底，避免线程池异常逃逸导致线程退出。
            }
        }
    }

    ThreadPoolOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool stopped_ = true;
    bool running_ = false;
};

} // namespace SZX::COMMON
