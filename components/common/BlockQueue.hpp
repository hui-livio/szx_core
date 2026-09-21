/**
 * @file BlockQueue.hpp
 * @brief 阻塞队列工具 — 提供线程间生产消费数据结构。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace SZX::COMMON
{

enum class QueuePushResult : unsigned char
{
    Pushed,
    Full,
    Closed,
};

template <typename T>
class BlockingQueue
{
public:
    explicit BlockingQueue(std::size_t capacity = 0) :
        capacity_(capacity)
    {
    }
    BlockingQueue(const BlockingQueue &)            = delete;
    BlockingQueue &operator=(const BlockingQueue &) = delete;

    BlockingQueue(BlockingQueue &&)            = delete;
    BlockingQueue &operator=(BlockingQueue &&) = delete;

    ~BlockingQueue()
    {
        close();
    }

public:
    // -----------------------------
    // 生产者接口
    // -----------------------------

    // 拷贝入队
    bool push(const T &value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] { return closed_ || !fullLocked(); });
        if (closed_)
        {
            return false;
        }
        queue_.push_back(value);
        updateHighWaterMarkLocked();
        not_empty_cv_.notify_one();
        return true;
    }

    // 移动入队
    bool push(T &&value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] { return closed_ || !fullLocked(); });
        if (closed_)
        {
            return false;
        }
        queue_.push_back(std::move(value));
        updateHighWaterMarkLocked();
        not_empty_cv_.notify_one();
        return true;
    }

    QueuePushResult try_push(const T &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_)
            return QueuePushResult::Closed;
        if (fullLocked())
            return QueuePushResult::Full;
        queue_.push_back(value);
        updateHighWaterMarkLocked();
        not_empty_cv_.notify_one();
        return QueuePushResult::Pushed;
    }

    QueuePushResult try_push(T &&value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_)
            return QueuePushResult::Closed;
        if (fullLocked())
            return QueuePushResult::Full;
        queue_.push_back(std::move(value));
        updateHighWaterMarkLocked();
        not_empty_cv_.notify_one();
        return QueuePushResult::Pushed;
    }

    // 原地构造
    template <typename... Args>
    bool emplace(Args &&...args)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] { return closed_ || !fullLocked(); });
        if (closed_)
        {
            return false;
        }
        queue_.emplace_back(std::forward<Args>(args)...);
        updateHighWaterMarkLocked();
        not_empty_cv_.notify_one();
        return true;
    }

    // -----------------------------
    // 消费者接口
    // -----------------------------

    // 阻塞弹出：
    // 成功返回 true，并把值写入 out
    // 当队列已关闭且无数据时，返回 false
    bool pop(T &out)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });

        if (queue_.empty())
        {
            // 只能是 closed_ == true 且已经没有数据
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return true;
    }

    // 返回 optional 版本
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });

        if (queue_.empty())
        {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return value;
    }

    // 非阻塞弹出
    bool try_pop(T &out)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
        {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return true;
    }

    // 超时阻塞弹出
    template <typename Rep, typename Period>
    bool pop_for(T &out, const std::chrono::duration<Rep, Period> &timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool ready = not_empty_cv_.wait_for(lock, timeout, [this] { return closed_ || !queue_.empty(); });

        if (!ready || queue_.empty())
        {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return true;
    }

    // 超时阻塞弹出，返回 optional
    template <typename Rep, typename Period>
    std::optional<T> pop_for(const std::chrono::duration<Rep, Period> &timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool ready = not_empty_cv_.wait_for(lock, timeout, [this] { return closed_ || !queue_.empty(); });

        if (!ready || queue_.empty())
        {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return value;
    }

    // -----------------------------
    // 状态控制
    // -----------------------------

    // 关闭队列：
    // 1. 后续 push/emplace 失败
    // 2. 唤醒所有等待中的消费者
    // 3. 已存在的数据仍可继续 pop
    void close()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_)
        {
            return;
        }
        closed_ = true;
        not_empty_cv_.notify_all();
        not_full_cv_.notify_all();
    }

    bool is_closed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    // 清空队列
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        not_full_cv_.notify_all();
    }

    // 当前元素个数
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

    std::size_t high_water_mark() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return high_water_mark_;
    }

    bool full() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return fullLocked();
    }

private:
    bool fullLocked() const
    {
        return capacity_ != 0 && queue_.size() >= capacity_;
    }

    void updateHighWaterMarkLocked()
    {
        high_water_mark_ = (std::max)(high_water_mark_, queue_.size());
    }

    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
    std::deque<T> queue_;
    std::size_t capacity_{0};
    std::size_t high_water_mark_{0};
    bool closed_{false};
};

} // namespace SZX::COMMON
