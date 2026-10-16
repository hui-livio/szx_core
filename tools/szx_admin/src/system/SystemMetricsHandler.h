#pragma once
#include "Result.hpp"
namespace SZX::ADMIN::INFRA
{
class HttpTransport;
class DatabasePool;
} // namespace SZX::ADMIN::INFRA
namespace SZX::ADMIN::AUTH
{
class LoginService;
}
namespace SZX::ADMIN::SYSTEM
{
inline namespace v1
{
class SystemMetricsService;
/** Dependencies must outlive transport callbacks. Enforces system.metrics.read on every request. */
[[nodiscard]] auto registerSystemMetricsRoutes(INFRA::HttpTransport &transport, INFRA::DatabasePool &database,
                                               AUTH::LoginService &login, SystemMetricsService &metrics)
    -> SZX::COMMON::Result<void>;
} // namespace v1
} // namespace SZX::ADMIN::SYSTEM
