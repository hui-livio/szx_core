#pragma once

#include <coroutine>

#include "OrmRuntime.h"

#include "Logger.h"

#include <ormpp/mysql.hpp>

#include <algorithm>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SZX::ADMIN::INFRA::DETAIL
{
// Private business adapter: parameter binding remains owned by ormpp. Driver
// diagnostics never reach stdout and error state is captured before unlocking.
class OrmConnection final
{
    template <typename Callback>
    auto invoke(Callback &&callback)
    {
        std::lock_guard lock(ormMutex());
        if (deadline_ && std::chrono::steady_clock::now() >= *deadline_)
            throw std::runtime_error("database operation deadline exceeded");
        const OrmDiagnosticsScope diagnostics;
        hasError_ = true;
        connection_.reset_error();
        connection_.set_enable_transaction(false);
        const auto started = std::chrono::steady_clock::now();
        auto result = callback();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
        if (elapsed >= std::chrono::microseconds{5000})
            LOGM_DEBUG("szx_admin.database", "event=orm_operation_slow duration_microseconds={}", elapsed.count());
        hasError_   = connection_.has_error();
        lastError_  = connection_.get_last_error();
        return result;
    }

public:
    OrmConnection()                                          = default;
    OrmConnection(const OrmConnection &)                     = delete;
    auto operator=(const OrmConnection &) -> OrmConnection & = delete;

    template <typename... Args>
    auto connect(Args &&...args) -> bool
    {
        return invoke([&] { return connection_.connect(std::forward<Args>(args)...); });
    }

    auto execute(const std::string &sql) -> bool
    {
        return invoke([&] { return connection_.execute(sql); });
    }

    auto ping() -> bool
    {
        return invoke([&] { return connection_.ping(); });
    }
    auto begin() -> bool
    {
        return invoke([&] { return connection_.begin(); });
    }
    auto commit() -> bool
    {
        return invoke([&] { return connection_.commit(); });
    }
    auto rollback() -> bool
    {
        return invoke([&] { return connection_.rollback(); });
    }
    [[nodiscard]] auto affectedRows() -> std::uint64_t
    {
        return static_cast<std::uint64_t>((std::max)(0, connection_.get_last_affect_rows()));
    }
    void setDeadline(std::optional<std::chrono::steady_clock::time_point> deadline) noexcept
    {
        deadline_ = deadline;
    }
    [[nodiscard]] auto deadlineExceeded() const noexcept -> bool
    {
        return deadline_ && std::chrono::steady_clock::now() >= *deadline_;
    }
    [[nodiscard]] auto lastError() const -> const std::string &
    {
        return lastError_;
    }
    void invalidate() noexcept
    {
        reusable_ = false;
    }
    [[nodiscard]] auto reusable() const noexcept -> bool
    {
        return reusable_;
    }
    auto has_error() const noexcept -> bool
    {
        return hasError_;
    }

    template <typename T, typename... Args>
    auto query_s(Args &&...args)
    {
        return invoke([&] { return connection_.query_s<T>(std::forward<Args>(args)...); });
    }

    template <typename T>
    auto insert(const T &value) -> int
    {
        return invoke([&] { return connection_.insert(value); });
    }

    template <typename T>
    auto insert(const std::vector<T> &values) -> int
    {
        return invoke([&] { return connection_.insert(values); });
    }

    template <auto... Members, typename T>
    auto update(const T &value) -> int
    {
        return invoke([&] { return connection_.update<Members...>(value); });
    }

private:
    ormpp::mysql connection_;
    bool hasError_{};
    std::string lastError_;
    std::optional<std::chrono::steady_clock::time_point> deadline_;
    bool reusable_{true};
};
} // namespace SZX::ADMIN::INFRA::DETAIL
