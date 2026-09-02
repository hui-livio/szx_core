/**
 * @file Rsa.h
 * @brief 基于 Windows CNG 的 RSA 密钥生成及 OAEP/PSS 操作。
 */
#pragma once

#include "Result.hpp"
#include "SzxCrypto/CryptoExport.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace SZX::CRYPTO
{

/** RSA 操作返回的稳定错误码。 */
enum class RsaError : int
{
    InvalidArgument      = 1710, ///< 输入参数或密钥格式无效。
    ProviderFailure      = 1711, ///< CNG provider 执行失败。
    AuthenticationFailed = 1712, ///< OAEP 解密或认证失败。
    Unsupported          = 1713, ///< 当前平台不支持该 provider。
};

/** CNG 导出的 RSA blob；privateKey 必须作为敏感数据保护。 */
struct RsaKeyPair
{
    std::vector<std::uint8_t> publicKey;
    std::vector<std::uint8_t> privateKey;
};

/** 生成 3072 位 RSA 密钥对，返回 CNG blob 格式的公钥和私钥。 */
[[nodiscard]] SZX_CRYPTO_API auto generateRsa3072KeyPair() -> SZX::COMMON::Result<RsaKeyPair>;

/** 根据 CNG 公钥 blob 计算稳定的小写十六进制 SHA-256 Key ID。 */
[[nodiscard]] SZX_CRYPTO_API auto rsaPublicKeyId(std::span<const std::uint8_t> publicKey) -> SZX::COMMON::Result<std::string>;
/** 使用 RSA-OAEP-SHA256 加密一条消息；大数据应使用混合加密。 */
[[nodiscard]] SZX_CRYPTO_API auto rsaOaepSha256Encrypt(std::span<const std::uint8_t> publicKey,
                                                       std::span<const std::uint8_t> plaintext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用 RSA-OAEP-SHA256 解密一条消息。 */
[[nodiscard]] SZX_CRYPTO_API auto rsaOaepSha256Decrypt(std::span<const std::uint8_t> privateKey,
                                                       std::span<const std::uint8_t> ciphertext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用 RSA-PSS-SHA256 对数据签名。 */
[[nodiscard]] SZX_CRYPTO_API auto rsaPssSha256Sign(std::span<const std::uint8_t> privateKey, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 验证 RSA-PSS-SHA256 签名；签名无效时返回 Success(false)。 */
[[nodiscard]] SZX_CRYPTO_API auto rsaPssSha256Verify(std::span<const std::uint8_t> publicKey, std::span<const std::uint8_t> data,
                                                     std::span<const std::uint8_t> signature) -> SZX::COMMON::Result<bool>;

} // namespace SZX::CRYPTO
