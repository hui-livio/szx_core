#pragma once

#include "SzxCryptoService/CryptoService.h"

#include <cstdint>
#include <map>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file JwtService.h
 * @brief RBAC JWT 签发、验证和密钥轮换接口。
 */
namespace SZX::ADMIN::AUTH
{
/** @brief JWT 服务返回的稳定错误码。 */
enum class JwtError : int
{
    InvalidArgument   = 2100, ///< 输入参数或服务配置无效。
    InvalidToken      = 2101, ///< JWT 格式或内容无法解析。
    AlgorithmRejected = 2102, ///< JWT 使用了不允许的算法。
    KeyRejected       = 2103, ///< 密钥不存在、未生效或已过期。
    SignatureInvalid  = 2104, ///< JWT 签名校验失败。
    ClaimsInvalid     = 2105, ///< JWT 必需声明缺失或值无效。
    CryptoFailure     = 2106, ///< 底层密码学操作失败。
    InternalFailure   = 2107, ///< 服务内部异常。
};

/**
 * @brief JWT 签发和验证配置。
 *
 * 时间字段使用秒；时钟偏差用于验证时的 exp、nbf 容忍窗口。
 */
struct JwtConfig
{
    std::string issuer;                  ///< 签发方标识，对应 iss 声明。
    std::string audience;                ///< 受众标识，对应 aud 声明。
    std::int64_t lifetimeSeconds{900};   ///< token 有效期，必须大于零。
    std::int64_t clockLeewaySeconds{30}; ///< 验证时允许的时钟偏差，不能为负数。
};

/** @brief JWT 签名和验证所需的密钥材料。 */
struct JwtSigningKey
{
    std::string keyId;                    ///< JWT header 中的 kid，必须唯一且非空。
    std::vector<std::uint8_t> privateKey; ///< PS256 私钥，仅用于签发。
    std::vector<std::uint8_t> publicKey;  ///< PS256 公钥，仅用于验证。
};

/** @brief JWT 签发请求中的业务身份和授权版本。 */
struct JwtIssueRequest
{
    std::string subject;          ///< 用户或主体标识，对应 sub 声明。
    std::string jti;              ///< token 唯一标识，对应 jti 声明。
    std::uint64_t authzVersion{}; ///< 授权版本，用于检测权限变更。
};

/** @brief JWT 验证成功后解析出的标准声明。 */
struct JwtClaims
{
    std::string keyId;            ///< 使用的密钥标识。
    std::string issuer;           ///< 签发方标识。
    std::string audience;         ///< 受众标识。
    std::string subject;          ///< 主体标识。
    std::string jti;              ///< token 唯一标识。
    std::int64_t issuedAt{};      ///< 签发时间，UTC Unix 时间戳。
    std::int64_t notBefore{};     ///< 生效时间，UTC Unix 时间戳。
    std::int64_t expiresAt{};     ///< 到期时间，UTC Unix 时间戳。
    std::uint64_t authzVersion{}; ///< 授权版本。
};

/**
 * @brief 密钥存储中的密钥记录。
 *
 * 密钥过期后不能用于验证；轮换只影响后续签发，不会立即使仍在有效期内的
 * 旧密钥失效。
 */
struct JwtKeyRecord
{
    JwtSigningKey key;        ///< 密钥材料和 kid。
    bool activeForSigning{};  ///< 是否作为当前签名密钥。
    std::int64_t notBefore{}; ///< 密钥生效时间；零表示不限制。
    std::int64_t expiresAt{}; ///< 密钥过期时间；零表示不限制。
};

/**
 * @brief 线程安全的内存 JWT 密钥存储。
 *
 * 该类适合作为基础能力或测试实现；生产环境仍需接入安全的密钥托管和
 * 持久化机制。
 */
class JwtKeyStore final
{
public:
    /**
     * @brief 创建绑定到密码学服务的密钥存储。
     * @param[in] crypto 用于计算公钥对应的稳定 keyId。
     */
    explicit JwtKeyStore(SZX::CRYPTO_SERVICE::ICryptoService &crypto);

    /**
     * @brief 添加密钥记录；同一 keyId 不能重复注册。
     *
     * keyId 必须等于密码学服务根据公钥计算出的稳定标识。
     */
    [[nodiscard]] auto addKey(JwtKeyRecord record) -> SZX::COMMON::Result<void>;

    /**
     * @brief 激活指定密钥作为后续 token 的签名密钥。
     * @param[in] keyId 要激活的密钥标识。
     * @param[in] now 校验有效期的 UTC Unix 时间戳；零表示使用当前时间。
     * @return 成功返回空结果，失败返回 KeyRejected 或 InternalFailure。
     */
    [[nodiscard]] auto activateKey(std::string_view keyId, std::int64_t now = 0) -> SZX::COMMON::Result<void>;

    /**
     * @brief 获取当前有效的签名密钥。
     * @param[in] now 校验有效期的 UTC Unix 时间戳；零表示使用当前时间。
     * @return 当前签名密钥，找不到时返回 KeyRejected。
     */
    [[nodiscard]] auto signingKey(std::int64_t now = 0) const -> SZX::COMMON::Result<JwtSigningKey>;

    /**
     * @brief 按 kid 获取当前有效的验证密钥。
     * @param[in] keyId JWT header 中的密钥标识。
     * @param[in] now 校验有效期的 UTC Unix 时间戳；零表示使用当前时间。
     * @return 验证密钥，找不到或过期时返回 KeyRejected。
     */
    [[nodiscard]] auto verificationKey(std::string_view keyId, std::int64_t now = 0) const -> SZX::COMMON::Result<JwtSigningKey>;

private:
    SZX::CRYPTO_SERVICE::ICryptoService &crypto_;
    mutable std::shared_mutex mutex_;
    std::map<std::string, JwtKeyRecord, std::less<>> keys_;
};

/**
 * @brief 使用 CryptoService 完成 PS256 JWT 签发和验证。
 *
 * 公共接口不暴露 jwt-cpp 类型；所有密码学操作均委托给 CryptoService。
 */
class JwtService final
{
public:
    /**
     * @brief 创建使用调用方显式提供密钥的 JWT 服务。
     * @param[in] crypto 密码学服务，必须在 JwtService 生命周期内保持有效。
     * @param[in] config JWT 签发和验证配置。
     */
    JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config);

    /**
     * @brief 创建使用密钥存储自动选择密钥的 JWT 服务。
     * @param[in] crypto 密码学服务，必须在 JwtService 生命周期内保持有效。
     * @param[in] config JWT 签发和验证配置。
     * @param[in] keyStore 密钥存储，必须在 JwtService 生命周期内保持有效。
     */
    JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config, JwtKeyStore &keyStore);

    /**
     * @brief 使用密钥存储中的当前签名密钥签发 JWT。
     * @param[in] request JWT 主体、jti 和授权版本。
     * @return 成功返回 compact JWT，失败返回稳定 JWT 错误码。
     */
    [[nodiscard]] auto issue(const JwtIssueRequest &request) -> SZX::COMMON::Result<std::string>;

    /**
     * @brief 根据 token 的 kid 从密钥存储中选择验证密钥并验证 JWT。
     * @param[in] token compact JWT 字符串。
     * @return 成功返回解析后的声明，失败返回稳定 JWT 错误码。
     */
    [[nodiscard]] auto verify(std::string_view token) -> SZX::COMMON::Result<JwtClaims>;

    /**
     * @brief 使用调用方提供的私钥签发 JWT。
     * @param[in] request JWT 主体、jti 和授权版本。
     * @param[in] key 签名密钥，必须包含私钥和非空 keyId。
     * @return 成功返回 compact JWT，失败返回稳定 JWT 错误码。
     */
    [[nodiscard]] auto issue(const JwtIssueRequest &request, const JwtSigningKey &key) -> SZX::COMMON::Result<std::string>;

    /**
     * @brief 使用调用方提供的公钥验证 JWT。
     * @param[in] token compact JWT 字符串。
     * @param[in] key 验证密钥，必须包含公钥且 keyId 与 token 的 kid 匹配。
     * @return 成功返回解析后的声明，失败返回稳定 JWT 错误码。
     */
    [[nodiscard]] auto verify(std::string_view token, const JwtSigningKey &key) -> SZX::COMMON::Result<JwtClaims>;

private:
    SZX::CRYPTO_SERVICE::ICryptoService &crypto_;
    JwtConfig config_;
    JwtKeyStore *keyStore_{};
};
} // namespace SZX::ADMIN::AUTH
