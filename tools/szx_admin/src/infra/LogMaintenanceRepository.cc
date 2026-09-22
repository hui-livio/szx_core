#include "LogMaintenanceRepository.h"

#include "OrmConnection.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <cctype>

namespace SZX::ADMIN::INFRA::DETAIL
{
namespace
{
using SZX::COMMON::Result;

template <typename T>
auto failed(ErrorCode code) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(code), {});
}

auto validCutoff(std::string_view value) -> bool
{
    if (value.size() != 26)
        return false;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const bool separator = (index == 4 || index == 7) ? value[index] == '-' :
            (index == 10)                                 ? value[index] == ' ' :
            (index == 13 || index == 16)                  ? value[index] == ':' :
            (index == 19)                                 ? value[index] == '.' :
                                                            std::isdigit(static_cast<unsigned char>(value[index])) != 0;
        if (!separator)
            return false;
    }
    return true;
}

auto purgeBatch(OrmConnection &connection, std::string_view table, std::string_view cutoff, std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogPurgeBatch>
{
    if ((table != "security_audit_logs" && table != "client_behavior_events") || !validCutoff(cutoff) || batchSize == 0)
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::ValidationError);
    if (!connection.execute(
            "CREATE TEMPORARY TABLE IF NOT EXISTS szx_log_purge_ids (id CHAR(36) NOT NULL PRIMARY KEY) ENGINE=MEMORY") ||
        !connection.execute("DELETE FROM szx_log_purge_ids") ||
        !connection.execute("INSERT INTO szx_log_purge_ids(id) SELECT id FROM " + std::string{table} + " WHERE created_at < '" +
                            std::string{cutoff} + "' ORDER BY created_at, id LIMIT " + std::to_string(batchSize)))
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseQueryFailed);

    const auto selected = connection.affectedRows();
    if (selected == 0)
        return Result<MAINTENANCE::LogPurgeBatch>::Success({});
    if (!connection.execute("DELETE t FROM " + std::string{table} + " t INNER JOIN szx_log_purge_ids p ON p.id = t.id"))
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseQueryFailed);
    const auto deleted = connection.affectedRows();
    if (deleted != selected)
        return failed<MAINTENANCE::LogPurgeBatch>(ErrorCode::DatabaseTransactionFailed);
    return Result<MAINTENANCE::LogPurgeBatch>::Success({.deleted = deleted});
}
} // namespace

auto MySqlLogMaintenanceRepository::purgeAuditBatch(std::string cutoff, std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogPurgeBatch>
{
    return purgeBatch(connection_, "security_audit_logs", cutoff, batchSize);
}

auto MySqlLogMaintenanceRepository::purgeBehaviorBatch(std::string cutoff, std::uint32_t batchSize)
    -> Result<MAINTENANCE::LogPurgeBatch>
{
    return purgeBatch(connection_, "client_behavior_events", cutoff, batchSize);
}
} // namespace SZX::ADMIN::INFRA::DETAIL
