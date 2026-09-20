#pragma once

#include "OrmRuntime.h"

#include <ormpp/mysql.hpp>

#include <string>
#include <utility>

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
        const OrmDiagnosticsScope diagnostics;
        hasError_ = true;
        connection_.reset_error();
        connection_.set_enable_transaction(false);
        auto result = callback();
        hasError_   = connection_.has_error();
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

    template <auto... Members, typename T>
    auto update(const T &value) -> int
    {
        return invoke([&] { return connection_.update<Members...>(value); });
    }

private:
    ormpp::mysql connection_;
    bool hasError_{};
};
} // namespace SZX::ADMIN::INFRA::DETAIL
