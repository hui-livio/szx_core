#pragma once

#include "Result.hpp"

#include <optional>
#include <string>

namespace SZX::ADMIN::AUTH
{
struct LoginUser
{
    std::string id;
    std::optional<std::string> uid;
    std::optional<std::string> username;
    std::string display_name;
    std::string password_hash;
    std::string role;
    std::string status;
    int version{};
    // Persisted pending-change flag. Only super_admin requires it; other roles may skip the prompt.
    bool force_password_change{};
    std::optional<std::string> temporary_credential_expires_at;
    int failed_login_count{};
    std::optional<std::string> locked_until;
    int authz_version{};
    std::optional<std::string> last_login_at;
    std::optional<std::string> password_changed_at;
    std::string created_at;
    std::string updated_at;
};

struct LoginRefresh
{
    std::string id;
    std::string user_id;
    std::string token_hash;
    std::string family_id;
    std::string expires_at;
    std::string ip;
    std::optional<std::string> replaced_by;
    std::optional<std::string> last_used_at;
    std::optional<std::string> revoked_at;
    std::optional<std::string> revocation_reason;
};

/** Transaction-scoped authentication persistence. Lookups lock the user until commit/rollback. */
class LoginRepository
{
public:
    virtual ~LoginRepository() = default;
    virtual auto find(const std::string &uid, const std::string &username) -> SZX::COMMON::Result<std::optional<LoginUser>> = 0;
    virtual auto findById(const std::string &id) -> SZX::COMMON::Result<std::optional<LoginUser>>                           = 0;
    virtual auto saveState(const LoginUser &user) -> SZX::COMMON::Result<void>                                              = 0;
    /** Revoke every unrevoked session of a user; reason is stored for diagnostics. */
    virtual auto revokeRefresh(const std::string &userId, const std::string &now, const std::string &reason)
        -> SZX::COMMON::Result<void>                                                   = 0;
    virtual auto insertRefresh(const LoginRefresh &token) -> SZX::COMMON::Result<void> = 0;
    // Locate the owner without a lock, then lock the user before locking/re-reading the token.
    virtual auto findRefresh(const std::string &hash, bool lock) -> SZX::COMMON::Result<std::optional<LoginRefresh>> = 0;
    virtual auto saveRefresh(const LoginRefresh &token) -> SZX::COMMON::Result<void>                                 = 0;
    virtual auto revokeFamily(const std::string &userId, const std::string &familyId, const std::string &now,
                              const std::string &reason) -> SZX::COMMON::Result<void>                                = 0;
};
} // namespace SZX::ADMIN::AUTH
