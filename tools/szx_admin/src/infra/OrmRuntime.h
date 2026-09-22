#pragma once

#include <mutex>

namespace SZX::ADMIN::INFRA::DETAIL
{
// The unmodified MySQL driver owns process-wide mutable state. Hold this gate
// across a complete connection callback, including rollback and reconnection.
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
