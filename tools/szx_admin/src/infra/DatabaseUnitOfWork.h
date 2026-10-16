#pragma once

#include "SzxAdmin/Infra/Database.h"

namespace SZX::ADMIN::AUTH
{
class LoginRepository;
}

namespace SZX::ADMIN::AUDIT
{
class AuditRepository;
}

namespace SZX::ADMIN::TELEMETRY
{
class BehaviorEventRepository;
}

namespace SZX::ADMIN::MAINTENANCE
{
class LogMaintenanceRepository;
}

namespace SZX::ADMIN::USER
{
class UserRepository;
}

namespace SZX::ADMIN::RBAC
{
class RoleRepository;
class PermissionRepository;
}

namespace SZX::ADMIN::INFRA
{
/**
 * @brief One borrowed database connection and its transaction boundary.
 *
 * The unit owns no connection. It only coordinates the lifetime of a transaction
 * and provides typed repository access for the callback that borrowed the connection.
 * Missing repositories remain explicit so lightweight test connections can expose
 * only the capability required by a service.
 */
class DatabaseUnitOfWork final
{
public:
    explicit DatabaseUnitOfWork(DatabaseConnection &connection) noexcept;
    ~DatabaseUnitOfWork();

    DatabaseUnitOfWork(const DatabaseUnitOfWork &)                     = delete;
    auto operator=(const DatabaseUnitOfWork &) -> DatabaseUnitOfWork & = delete;

    [[nodiscard]] auto begin() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto commit() -> SZX::COMMON::Result<void>;
    void rollback() noexcept;
    [[nodiscard]] auto inTransaction() const noexcept -> bool;

    [[nodiscard]] auto users() noexcept -> USER::UserRepository *;
    [[nodiscard]] auto sessions() noexcept -> AUTH::LoginRepository *;
    [[nodiscard]] auto audit() noexcept -> AUDIT::AuditRepository *;
    [[nodiscard]] auto behaviorEvents() noexcept -> TELEMETRY::BehaviorEventRepository *;
    [[nodiscard]] auto logMaintenance() noexcept -> MAINTENANCE::LogMaintenanceRepository *;
    [[nodiscard]] auto roles() noexcept -> RBAC::RoleRepository *;
    [[nodiscard]] auto permissions() noexcept -> RBAC::PermissionRepository *;

private:
    DatabaseConnection &connection_;
    bool inTransaction_{};
};
} // namespace SZX::ADMIN::INFRA
