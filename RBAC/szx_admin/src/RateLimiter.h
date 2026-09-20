#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace SZX::ADMIN
{
/** Bounded, thread-safe fixed windows for one operation family. Uses monotonic time. */
class RateLimiter final
{
public:
    using Clock = std::chrono::steady_clock;
    explicit RateLimiter(int limit) :
        limit_(limit)
    {
    }

    auto admit(const std::string &key, Clock::time_point now = Clock::now()) -> bool
    {
        std::lock_guard lock(mutex_);
        std::erase_if(windows_, [&](const auto &item) { return item.second.expires <= now; });
        auto found = windows_.find(key);
        if (found == windows_.end())
        {
            if (windows_.size() >= 4096)
                return false;
            found = windows_.emplace(key, Window{now + std::chrono::minutes{1}, 0}).first;
        }
        if (found->second.count >= limit_)
            return false;
        ++found->second.count;
        return true;
    }

private:
    struct Window
    {
        Clock::time_point expires;
        int count{};
    };
    int limit_;
    std::mutex mutex_;
    std::map<std::string, Window> windows_;
};
} // namespace SZX::ADMIN
