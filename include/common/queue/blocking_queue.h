#pragma once

#include <condition_variable>
#include <cstddef>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace SZXCamera{
    
    template <typename T>
    class BlockingQueue {
        public:
            BlockingQueue() = default;
            BlockingQueue(const BlockingQueue&) = delete;
            BlockingQueue& operator=(const BlockingQueue&) = delete;

            BlockingQueue(BlockingQueue&&) = delete;
            BlockingQueue& operator=(BlockingQueue&&) = delete;

            ~BlockingQueue() {
                close();
            }

        public:
            // -----------------------------
            // 生产者接口
            // -----------------------------

            // 拷贝入队
            bool push(const T& value) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return false;
                }
                queue_.push_back(value);
                not_empty_cv_.notify_one();
                return true;
            }

            // 移动入队
            bool push(T&& value) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return false;
                }
                queue_.push_back(std::move(value));
                not_empty_cv_.notify_one();
                return true;
            }

            // 原地构造
            template <typename... Args>
            bool emplace(Args&&... args) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return false;
                }
                queue_.emplace_back(std::forward<Args>(args)...);
                not_empty_cv_.notify_one();
                return true;
            }

            // -----------------------------
            // 消费者接口
            // -----------------------------

            // 阻塞弹出：
            // 成功返回 true，并把值写入 out
            // 当队列已关闭且无数据时，返回 false
            bool pop(T& out) {
                std::unique_lock<std::mutex> lock(mutex_);
                not_empty_cv_.wait(lock, [this] {
                    return closed_ || !queue_.empty();
                });

                if (queue_.empty()) {
                    // 只能是 closed_ == true 且已经没有数据
                    return false;
                }

                out = std::move(queue_.front());
                queue_.pop_front();
                return true;
            }

            // 返回 optional 版本
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
                return value;
            }

            // 非阻塞弹出
            bool try_pop(T& out) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (queue_.empty()) {
                    return false;
                }

                out = std::move(queue_.front());
                queue_.pop_front();
                return true;
            }

            // 超时阻塞弹出
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
                return true;
            }

            // 超时阻塞弹出，返回 optional
            template <typename Rep, typename Period>
            std::optional<T> pop_for(const std::chrono::duration<Rep, Period>& timeout) {
                std::unique_lock<std::mutex> lock(mutex_);
                const bool ready = not_empty_cv_.wait_for(lock, timeout, [this] {
                    return closed_ || !queue_.empty();
                });

                if (!ready || queue_.empty()) {
                    return std::nullopt;
                }

                T value = std::move(queue_.front());
                queue_.pop_front();
                return value;
            }

            // -----------------------------
            // 状态控制
            // -----------------------------

            // 关闭队列：
            // 1. 后续 push/emplace 失败
            // 2. 唤醒所有等待中的消费者
            // 3. 已存在的数据仍可继续 pop
            void close() {
                std::lock_guard<std::mutex> lock(mutex_);
                if (closed_) {
                    return;
                }
                closed_ = true;
                not_empty_cv_.notify_all();
            }

            bool is_closed() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return closed_;
            }

            // 清空队列
            void clear() {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.clear();
            }

            // 当前元素个数
            std::size_t size() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return queue_.size();
            }

            bool empty() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return queue_.empty();
            }

        private:
            mutable std::mutex mutex_;
            std::condition_variable not_empty_cv_;
            std::deque<T> queue_;
            bool closed_{false};
    };

}