#pragma once

#include "Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SZX::ADMIN::USER
{
/**
 * @brief users 行投影。
 *
 * 故意不包含 password_hash：用户管理模块从不读取凭据，重置密码只写入。
 */
struct UserRow final
{
    std::string id;
    std::string login_identifier;
    std::string login_identifier_canonical;
    std::string identifier_type;
    std::string display_name;
    std::optional<std::string> role_id;
    std::string role;
    std::string status;
    int version{};
    bool force_password_change{};
    std::optional<std::string> temporary_credential_expires_at;
    int failed_login_count{};
    std::optional<std::string> locked_until;
    std::optional<std::string> password_changed_at;
    std::optional<std::string> disabled_at;
    std::optional<std::string> created_by;
    int authz_version{};
    std::optional<std::string> last_login_at;
    std::string created_at;
    std::string updated_at;
};

/** 创建用户的插入投影；未列出的列使用数据库默认值。 */
struct NewUser final
{
    std::string id;
    std::string login_identifier;
    std::string login_identifier_canonical;
    std::string identifier_type;
    std::string display_name;
    std::string password_hash;
    std::optional<std::string> role_id;
    std::string role;
    std::string status;
    bool force_password_change{};
    std::optional<std::string> temporary_credential_expires_at;
    std::string created_by;
};

/** 列表排序白名单；SQL 片段来自编译期常量，不接受请求文本。 */
enum class UserSort : int
{
    CreatedAtDesc,
    CreatedAtAsc,
    DisplayNameAsc,
    DisplayNameDesc,
    LastLoginAtDesc,
    LastLoginAtAsc,
    RoleAsc,
    StatusAsc,
};

struct UserFilter final
{
    /** 调用方可见的角色白名单；由 Policy::mayTarget 与请求 role 参数求交后得到，不得为空。 */
    std::vector<std::string> roles;
    std::string status; ///< 空表示不过滤。
    std::string search; ///< 空表示不过滤；仓储负责转义 LIKE 通配符。
    UserSort sort{UserSort::CreatedAtDesc};
    std::uint32_t limit{20};
    std::uint64_t offset{};
};

struct UserPage final
{
    std::vector<UserRow> items;
    std::uint64_t total{};
};

/** 一次加锁读取的结果：调用者、目标以及最后一个超管保护所需的计数。 */
struct MutationLock final
{
    std::optional<UserRow> actor;
    std::optional<UserRow> target;
    /** 已加锁的 active 超管行数，包含 target 自身。 */
    std::uint64_t activeSuperAdmins{};
};

/**
 * @brief 事务域内的用户与审计读写。
 *
 * 审计写入和 refresh 撤销复用同一连接的 AUTH::LoginRepository，保证与用户变更同事务提交。
 */
class UserRepository
{
public:
    virtual ~UserRepository() = default;

    /**
     * @brief 在单条按 id 排序的加锁读取中取得调用者、目标和全部 active 超管行。
     *
     * 一次性获取本次变更需要的所有行锁，且顺序由 id 决定，因此互为目标的
     * 并发请求不会死锁；actorId == targetId 时 actor 与 target 为同一行。
     */
    virtual auto lockForMutation(const std::string &actorId, const std::string &targetId)
        -> SZX::COMMON::Result<MutationLock> = 0;
    /** 创建用户时锁定调用者至事务结束；只读接口保持 lock=false。 */
    virtual auto findById(const std::string &id, bool lock = false) -> SZX::COMMON::Result<std::optional<UserRow>> = 0;
    /** 校验同字段及跨字段登录冲突；调用者须先锁定 bootstrap 单例行直至事务结束。 */
    virtual auto findByCanonical(const std::string &canonicalValue)
        -> SZX::COMMON::Result<std::optional<UserRow>>                           = 0;
    virtual auto list(const UserFilter &filter) -> SZX::COMMON::Result<UserPage> = 0;
    virtual auto insert(const NewUser &user) -> SZX::COMMON::Result<void>        = 0;
    /** 写回可变业务列；不触碰 password_hash 和身份标识。 */
    virtual auto save(const UserRow &user) -> SZX::COMMON::Result<void>                                              = 0;
    virtual auto saveCredential(const std::string &id, const std::string &passwordHash) -> SZX::COMMON::Result<void> = 0;
};
} // namespace SZX::ADMIN::USER
