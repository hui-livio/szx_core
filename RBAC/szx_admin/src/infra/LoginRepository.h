#pragma once

#include "../auth/LoginRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;
class MySqlLoginRepository final : public AUTH::LoginRepository
{
public:
    explicit MySqlLoginRepository(OrmConnection &connection) :
        connection_(connection)
    {
    }
    auto find(const std::string &uid, const std::string &username)
        -> SZX::COMMON::Result<std::optional<AUTH::LoginUser>> override;
    auto saveState(const AUTH::LoginUser &user) -> SZX::COMMON::Result<void> override;
    auto findById(const std::string &id) -> SZX::COMMON::Result<std::optional<AUTH::LoginUser>> override;
    auto revokeRefresh(const std::string &userId, const std::string &now, const std::string &reason)
        -> SZX::COMMON::Result<void> override;
    auto insertRefresh(const AUTH::LoginRefresh &token) -> SZX::COMMON::Result<void> override;
    auto findRefresh(const std::string &hash, bool lock) -> SZX::COMMON::Result<std::optional<AUTH::LoginRefresh>> override;
    auto saveRefresh(const AUTH::LoginRefresh &token) -> SZX::COMMON::Result<void> override;
    auto revokeFamily(const std::string &userId, const std::string &familyId, const std::string &now, const std::string &reason)
        -> SZX::COMMON::Result<void> override;
private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
