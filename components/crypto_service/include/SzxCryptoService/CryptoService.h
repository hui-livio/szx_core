/** @file CryptoService.h @brief 统一加密服务门面。 */
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Result.hpp"
#include "SzxCryptoService/CryptoServiceExport.h"
namespace SZX::CRYPTO_SERVICE
{
/** CryptoService 对外返回的稳定错误码。 */
enum class CryptoServiceError : int
{
    InternalFailure      = 1600, ///< 服务边界捕获到未分类的内部异常。
    InvalidArgument      = 1601, ///< 输入参数或密钥格式无效。
    AuthenticationFailed = 1602, ///< 密码学认证失败。
    Unsupported          = 1603, ///< 当前密码 provider 不支持所请求算法。
    ProviderFailure      = 1604, ///< 底层密码 provider 操作失败。
    MalformedHash        = 1605  ///< 密码哈希编码格式无效。
};

/** AES-GCM 加密结果，nonce、密文和认证标签均为二进制字节序列。 */
struct CryptoCiphertext
{
    std::vector<std::uint8_t> nonce;
    std::vector<std::uint8_t> ciphertext;
    std::vector<std::uint8_t> tag;
};

/** 当前加密服务实例支持的算法能力。 */
struct CryptoCapabilities
{
    bool aes256Gcm{};
    bool rsaOaepSha256{};
    bool rsaPssSha256{};
    bool pbkdf2Sha256{};
    bool argon2id{};
    bool ps256Jwt{};
    bool publicKeyId{};
};

/** RBAC 使用的 Argon2id 参数；memoryCostKiB 为 1 KiB 内存块数量。 */
struct Argon2idParameters
{
    std::uint32_t memoryCostKiB{65536};
    std::uint32_t iterations{3};
    std::uint32_t lanes{1};
    std::size_t saltLength{16};
    std::size_t hashLength{32};
};

/** 上层业务使用的统一加密服务抽象。实现不得让异常穿过此边界。 */
class SZX_CRYPTO_SERVICE_API ICryptoService
{
public:
    virtual ~ICryptoService() = default;

    /** 返回服务支持的算法能力集合。 */
    virtual auto capabilities() const -> CryptoCapabilities = 0;

    /** 使用 AES-256-GCM 加密数据；aad 参与认证但不产生密文。 */
    virtual auto encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<CryptoCiphertext> = 0;

    /** 使用 AES-256-GCM 解密并认证数据；认证失败时不得返回明文。 */
    virtual auto decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    /** 使用 RSA-OAEP-SHA256 加密数据。 */
    virtual auto rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    /** 使用 RSA-OAEP-SHA256 解密数据。 */
    virtual auto rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    /** 使用 RSA-PSS-SHA256 对数据签名。 */
    virtual auto sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    /** 使用 RSA-PSS-SHA256 验证签名。 */
    virtual auto verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> = 0;

    /** 使用 PBKDF2-HMAC-SHA256 派生指定长度密钥。 */
    virtual auto derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    /** 以 Argon2id PHC 格式生成密码哈希。 */
    virtual auto hashPassword(std::span<const std::uint8_t>, const Argon2idParameters & = {}) -> SZX::COMMON::Result<std::string>
    {
        return SZX::COMMON::Result<std::string>::Failure(static_cast<int>(CryptoServiceError::Unsupported),
                                                         "当前加密服务未提供 Argon2id。");
    }

    /** 校验 Argon2id PHC 密码哈希；错误密码返回 Success(false)。 */
    virtual auto verifyPassword(std::span<const std::uint8_t>, std::string_view) -> SZX::COMMON::Result<bool>
    {
        return SZX::COMMON::Result<bool>::Failure(static_cast<int>(CryptoServiceError::Unsupported),
                                                  "当前加密服务未提供 Argon2id。");
    }

    /** 计算规范 DER RSA 公钥的稳定 kid（SHA-256 小写十六进制）。 */
    virtual auto publicKeyId(std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::string>
    {
        return SZX::COMMON::Result<std::string>::Failure(static_cast<int>(CryptoServiceError::Unsupported),
                                                         "当前加密服务未提供公钥 ID。");
    }

    /** PS256（RSA-PSS-SHA256）签名门面，等价于 sign。 */
    virtual auto signPs256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>>
    {
        return sign(key, data);
    }

    /** PS256 验签门面，等价于 verify。 */
    virtual auto verifyPs256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data,
                             std::span<const std::uint8_t> signature) -> SZX::COMMON::Result<bool>
    {
        return verify(key, data, signature);
    }
};

class SZX_CRYPTO_SERVICE_API CryptoService final : public ICryptoService
{
public:
    auto capabilities() const -> CryptoCapabilities override;

    auto encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<CryptoCiphertext> override;

    auto decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> override;

    auto derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto hashPassword(std::span<const std::uint8_t>, const Argon2idParameters & = {})
        -> SZX::COMMON::Result<std::string> override;

    auto verifyPassword(std::span<const std::uint8_t>, std::string_view) -> SZX::COMMON::Result<bool> override;

    auto publicKeyId(std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::string> override;

    auto signPs256(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto verifyPs256(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> override;
};

class SZX_CRYPTO_SERVICE_API FakeCryptoService final : public ICryptoService
{
public:
    CryptoCapabilities supported{};
    std::size_t encryptCalls{};
    std::size_t decryptCalls{};
    auto capabilities() const -> CryptoCapabilities override
    {
        return supported;
    }

    auto encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<CryptoCiphertext> override;

    auto decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> override;

    auto derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;
    auto hashPassword(std::span<const std::uint8_t>, const Argon2idParameters & = {})
        -> SZX::COMMON::Result<std::string> override;
    auto verifyPassword(std::span<const std::uint8_t>, std::string_view) -> SZX::COMMON::Result<bool> override;
    auto publicKeyId(std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::string> override;
    auto signPs256(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;
    auto verifyPs256(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> override;
};
} // namespace SZX::CRYPTO_SERVICE
/** 使用 AES-256-GCM 解密并认证数据；认证失败时不得返回明文。 */
/** 使用 RSA-OAEP-SHA256 加密数据。 */
/** 使用 RSA-OAEP-SHA256 解密数据。 */
/** 使用 RSA-PSS-SHA256 对数据签名。 */
/** 使用 RSA-PSS-SHA256 验证签名。 */
/** 使用 PBKDF2-HMAC-SHA256 派生指定长度密钥。 */
