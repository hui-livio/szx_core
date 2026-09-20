#pragma once

#include "../RateLimiter.h"
#include "UserRepository.h"

#include "SzxAdmin/Infra/Database.h"
#include "SzxAdmin/Policy/Policy.h"
#include "SzxCryptoService/CryptoService.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SZX::ADMIN::AUTH
{
class LoginService;
}

namespace SZX::ADMIN::USER
{
/** 每个受保护请求携带的调用者上下文。peerAddress 用于审计，不信任转发请求头。 */
struct ActorContext final
{
    std::string accessToken;
    std::string requestId;
    std::string peerAddress;
};

struct ListQuery final
{
    std::string role;   ///< 空表示调用者可见的全部角色。
    std::string status; ///< 空表示不过滤。
    std::string search; ///< 空表示不过滤。
    std::string sort;   ///< 空表示默认 -created_at；非白名单值返回 2206。
    std::uint32_t page{1};
    std::uint32_t pageSize{20};
};

/** 创建请求必须且只能提供 uid 或 username。 */
struct CreateUserInput final
{
    std::optional<std::string> uid;
    std::optional<std::string> username;
    std::string displayName;
    std::string role;
    std::optional<std::string> passwordHash; ///< 可选的客户端 SHA-256 摘要；省略时生成临时密码。
};

/** 创建和重置密码的结果；临时凭据明文只在本次响应中出现，不落库。 */
struct CredentialIssue final
{
    UserRow user;
    std::string temporaryCredential;
    std::string temporaryCredentialExpiresAt; ///< UTC ISO 8601。
};

struct LogQuery final
{
    std::string from;        ///< UTC ISO 8601；空表示不过滤。
    std::string to;          ///< 同上。
    std::string eventName;   ///< 精确匹配。
    std::string result;      ///< success/failure/denied。
    std::string actorUserId; ///< UUID。
    std::uint32_t page{1};
    std::uint32_t pageSize{20};
    std::string actorSearch; ///< 展示名、用户名或 UID 子串，最多 128 个字符。
};

struct Permissions final
{
    std::string userId;
    std::string role;
    std::string status;
    bool forcePasswordChange{};
    int authzVersion{};
    std::vector<std::string_view> capabilities;
};

/** 非可信客户端埋点；不得用于记录安全审计事件。 */
struct BehaviorEvent final
{
    std::string eventName;
    std::string result;
    std::string detailsJson; ///< 控制器已校验的 JSON 对象文本。
};

/**
 * @brief 用户生命周期、固定角色分配和审计查询。
 *
 * 每个方法都自行校验 access token、当前用户状态、当前 authz_version、能力码、
 * 目标角色和目标状态；不依赖 JWT 中的角色声明。写操作在单个事务内完成
 * 状态变更、refresh 撤销和审计，任一失败整体回滚。
 * Database 和 LoginService 必须比本服务存活更久。
 */
class UserService final
{
public:
    UserService(INFRA::DatabasePool &database, AUTH::LoginService &auth);

    [[nodiscard]] auto permissions(const ActorContext &actor) -> SZX::COMMON::Result<Permissions>;
    [[nodiscard]] auto list(const ActorContext &actor, const ListQuery &query) -> SZX::COMMON::Result<UserPage>;
    [[nodiscard]] auto get(const ActorContext &actor, const std::string &id) -> SZX::COMMON::Result<UserRow>;
    [[nodiscard]] auto create(const ActorContext &actor, const CreateUserInput &input) -> SZX::COMMON::Result<CredentialIssue>;
    /** 只更新 display_name；uid、username 和 role 创建后不可改。 */
    [[nodiscard]] auto updateProfile(const ActorContext &actor, const std::string &id, const std::string &displayName,
                                     std::uint32_t version) -> SZX::COMMON::Result<UserRow>;
    /** active/disabled 切换；禁用会撤销目标的全部 refresh 会话。 */
    [[nodiscard]] auto changeStatus(const ActorContext &actor, const std::string &id, const std::string &status,
                                    std::uint32_t version) -> SZX::COMMON::Result<UserRow>;
    /** 仅允许 locked 转 active。 */
    [[nodiscard]] auto unlock(const ActorContext &actor, const std::string &id, std::uint32_t version)
        -> SZX::COMMON::Result<UserRow>;
    [[nodiscard]] auto assignRole(const ActorContext &actor, const std::string &id, const std::string &role,
                                  std::uint32_t version) -> SZX::COMMON::Result<UserRow>;
    [[nodiscard]] auto resetPassword(const ActorContext &actor, const std::string &id, std::uint32_t version)
        -> SZX::COMMON::Result<CredentialIssue>;
    [[nodiscard]] auto logs(const ActorContext &actor, const LogQuery &query) -> SZX::COMMON::Result<LogPage>;
    [[nodiscard]] auto recordBehavior(const ActorContext &actor, const BehaviorEvent &event) -> SZX::COMMON::Result<std::string>;

private:
    /** 一次加锁授权的结果：调用者、目标和已加锁的 active 超管计数。 */
    struct Mutation final
    {
        UserRow actor;
        UserRow target;
        std::uint64_t activeSuperAdmins{};
    };

    /** 校验 token 和数据库当前状态；写操作须在事务内令 lock=true。 */
    auto resolve(const ActorContext &context, UserRepository &repository, bool lock = false) -> SZX::COMMON::Result<UserRow>;
    /** 在已开启的事务中锁定调用者、目标及 active 超管行，并校验能力、目标角色和版本。 */
    auto authorize(const ActorContext &context, UserRepository &repository, const std::string &targetId,
                   POLICY::Capability required, std::uint32_t version) -> SZX::COMMON::Result<Mutation>;
    /** 生成一次性临时凭据，返回 {明文, 可存储的 Argon2id 记录}。 */
    auto issueTemporaryCredential() -> SZX::COMMON::Result<std::pair<std::string, std::string>>;

    INFRA::DatabasePool &database_;
    AUTH::LoginService &auth_;
    SZX::CRYPTO_SERVICE::CryptoService crypto_;
    RateLimiter behaviorLimiter_;
};
} // namespace SZX::ADMIN::USER
