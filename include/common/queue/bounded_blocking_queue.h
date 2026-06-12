#pragma once

#include <condition_variable>
#include <cstddef>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

template <typename T>
class BoundedBlockingQueue {
public:
    explicit BoundedBlockingQueue(std::size_t capacity)
        : capacity_(capacity)
    {
        if (capacity_ == 0) {
            throw std::invalid_argument("capacity must be greater than 0");
        }
    }

    BoundedBlockingQueue(const BoundedBlockingQueue&) = delete;
    BoundedBlockingQueue& operator=(const BoundedBlockingQueue&) = delete;

    ~BoundedBlockingQueue() {
        close();
    }

public:
    // 阻塞入队。
    // 当队列满时等待，直到：
    // 1. 有空位
    // 2. 或队列被 close()
    bool push(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] {
            return closed_ || queue_.size() < capacity_;
        });

        if (closed_) {
            return false;
        }

        queue_.push_back(value);
        not_empty_cv_.notify_one();
        return true;
    }

    bool push(T&& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] {
            return closed_ || queue_.size() < capacity_;
        });

        if (closed_) {
            return false;
        }

        queue_.push_back(std::move(value));
        not_empty_cv_.notify_one();
        return true;
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this] {
            return closed_ || queue_.size() < capacity_;
        });

        if (closed_) {
            return false;
        }

        queue_.emplace_back(std::forward<Args>(args)...);
        not_empty_cv_.notify_one();
        return true;
    }

    // 非阻塞入队。
    // 若队列已满或已关闭，立即返回 false。
    bool try_push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) {
            return false;
        }

        queue_.push_back(value);
        not_empty_cv_.notify_one();
        return true;
    }

    bool try_push(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) {
            return false;
        }

        queue_.push_back(std::move(value));
        not_empty_cv_.notify_one();
        return true;
    }

    template <typename Rep, typename Period>
    bool push_for(T value, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        const bool ready = not_full_cv_.wait_for(lock, timeout, [this] {
            return closed_ || queue_.size() < capacity_;
        });

        if (!ready || closed_) {
            return false;
        }

        queue_.push_back(std::move(value));
        not_empty_cv_.notify_one();
        return true;
    }

    // 阻塞出队。
    // 当队列为空时等待，直到：
    // 1. 有数据
    // 2. 或队列被 close()
    //
    // 返回 false 表示：
    // - 队列已关闭
    // - 且已经没有剩余数据可取
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait(lock, [this] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T value = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return value;
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return true;
    }

    template <typename Rep, typename Period>
    bool pop_for(T& out, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        const bool ready = not_empty_cv_.wait_for(lock, timeout, [this] {
            return closed_ || !queue_.empty();
        });

        if (!ready || queue_.empty()) {
            return false;
        }

        out = std::move(queue_.front());
        queue_.pop_front();
        not_full_cv_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            return;
        }

        closed_ = true;
        not_empty_cv_.notify_all();
        not_full_cv_.notify_all();
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t capacity() const {
        return capacity_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
    std::deque<T> queue_;
    std::size_t capacity_;
    bool closed_{false};
};
