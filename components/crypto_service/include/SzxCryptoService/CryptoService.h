/** @file CryptoService.h @brief 统一加密服务门面。 */
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include "Result.hpp"
#include "SzxCryptoService/CryptoServiceExport.h"
namespace SZX::CRYPTO_SERVICE
{
/** CryptoService 对外返回的稳定错误码。 */
enum class CryptoServiceError : int
{
    InternalFailure = 1600 ///< 服务边界捕获到未分类的内部异常。
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
};
} // namespace SZX::CRYPTO_SERVICE
/** 使用 AES-256-GCM 解密并认证数据；认证失败时不得返回明文。 */
/** 使用 RSA-OAEP-SHA256 加密数据。 */
/** 使用 RSA-OAEP-SHA256 解密数据。 */
/** 使用 RSA-PSS-SHA256 对数据签名。 */
/** 使用 RSA-PSS-SHA256 验证签名。 */
/** 使用 PBKDF2-HMAC-SHA256 派生指定长度密钥。 */
