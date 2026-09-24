#include "DatabaseUnitOfWork.h"

#include "SzxAdmin/Infra/ErrorCode.h"

namespace SZX::ADMIN::INFRA
{
DatabaseUnitOfWork::DatabaseUnitOfWork(DatabaseConnection &connection) noexcept :
    connection_(connection)
{
}

DatabaseUnitOfWork::~DatabaseUnitOfWork()
{
    rollback();
}

auto DatabaseUnitOfWork::begin() -> SZX::COMMON::Result<void>
{
    if (inTransaction_)
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseTransactionFailed), {});
    const auto result = connection_.begin();
    if (result.ok)
        inTransaction_ = true;
    return result;
}

auto DatabaseUnitOfWork::commit() -> SZX::COMMON::Result<void>
{
    if (!inTransaction_)
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(ErrorCode::DatabaseTransactionFailed), {});
    const auto result = connection_.commit();
    if (result.ok)
        inTransaction_ = false;
    return result;
}

void DatabaseUnitOfWork::rollback() noexcept
{
    if (!inTransaction_)
        return;
    try
    {
        (void)connection_.rollback();
    }
    catch (...)
    {
    }
    inTransaction_ = false;
}

auto DatabaseUnitOfWork::inTransaction() const noexcept -> bool
{
    return inTransaction_;
}

auto DatabaseUnitOfWork::users() noexcept -> USER::UserRepository *
{
    return connection_.userRepository();
}

auto DatabaseUnitOfWork::sessions() noexcept -> AUTH::LoginRepository *
{
    return connection_.loginRepository();
}

auto DatabaseUnitOfWork::audit() noexcept -> AUDIT::AuditRepository *
{
    return connection_.auditRepository();
}

auto DatabaseUnitOfWork::behaviorEvents() noexcept -> TELEMETRY::BehaviorEventRepository *
{
    return connection_.behaviorEventRepository();
}

auto DatabaseUnitOfWork::logMaintenance() noexcept -> MAINTENANCE::LogMaintenanceRepository *
{
    return connection_.logMaintenanceRepository();
}
} // namespace SZX::ADMIN::INFRA
