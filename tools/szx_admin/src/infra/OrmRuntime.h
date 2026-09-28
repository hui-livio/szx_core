#pragma once

#include <mutex>

namespace SZX::ADMIN::INFRA::DETAIL
{
// Process-wide driver setup is serialized during startup; ordinary operations
// are serialized only by the mutex owned by each pooled connection.
auto ormMutex() -> std::recursive_timed_mutex &;

class OrmDiagnosticsScope final
{
public:
    OrmDiagnosticsScope() noexcept;
    ~OrmDiagnosticsScope();
    OrmDiagnosticsScope(const OrmDiagnosticsScope &)                     = delete;
    auto operator=(const OrmDiagnosticsScope &) -> OrmDiagnosticsScope & = delete;

private:
    bool previous_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
