#pragma once

#include "../user/UserRepository.h"

namespace SZX::ADMIN::INFRA::DETAIL
{
class OrmConnection;
class MySqlUserRepository final : public USER::UserRepository
{
public:
    explicit MySqlUserRepository(OrmConnection &connection) :
        connection_(connection)
    {
    }
    auto lockForMutation(const std::string &actorId, const std::string &targetId)
        -> SZX::COMMON::Result<USER::MutationLock> override;
    auto findById(const std::string &id, bool lock = false) -> SZX::COMMON::Result<std::optional<USER::UserRow>> override;
    auto findByCanonical(const std::string &uid, const std::string &username)
        -> SZX::COMMON::Result<std::optional<USER::UserRow>> override;
    auto list(const USER::UserFilter &filter) -> SZX::COMMON::Result<USER::UserPage> override;
    auto insert(const USER::NewUser &user) -> SZX::COMMON::Result<void> override;
    auto save(const USER::UserRow &user) -> SZX::COMMON::Result<void> override;
    auto saveCredential(const std::string &id, const std::string &passwordHash) -> SZX::COMMON::Result<void> override;
private:
    OrmConnection &connection_;
};
} // namespace SZX::ADMIN::INFRA::DETAIL
