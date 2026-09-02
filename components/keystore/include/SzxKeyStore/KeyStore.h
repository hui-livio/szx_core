/** @file KeyStore.h @brief Windows NCrypt 持久化密钥存储接口。 */

#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Result.hpp"
#include "SzxKeyStore/KeyStoreExport.h"

namespace SZX::KEYSTORE
{
enum class KeyStoreError : int
{
    InvalidArgument = 1800,
    ProviderFailure = 1801,
    NotFound        = 1802,
    Unsupported     = 1803
};
enum class KeyUsage : std::uint32_t
{
    Sign           = 1,
    Decrypt        = 2,
    SignAndDecrypt = 3
};
struct KeyReference
{
    std::string provider;
    std::string name;
};
/** 创建持久化 RSA-3072 密钥，并返回 CNG 公钥 blob。 */
[[nodiscard]] SZX_KEYSTORE_API auto createRsa3072(const std::string &name) -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 创建带用途限制的持久化 RSA-3072 密钥。 */
[[nodiscard]] SZX_KEYSTORE_API auto createRsa3072(const std::string &name, KeyUsage usage)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 根据引用打开已有 RSA 密钥并导出公钥 blob。 */
[[nodiscard]] SZX_KEYSTORE_API auto openRsaPublicKey(const KeyReference &reference)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 删除指定名称的持久化密钥。 */
[[nodiscard]] SZX_KEYSTORE_API auto deleteKey(const KeyReference &reference) -> SZX::COMMON::Result<void>;
/** 使用持久化私钥执行 RSA-PSS-SHA256 签名；私钥不会导出。 */
[[nodiscard]] SZX_KEYSTORE_API auto signPssSha256(const KeyReference &reference, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用持久化私钥执行 RSA-OAEP-SHA256 解密；私钥不会导出。 */
[[nodiscard]] SZX_KEYSTORE_API auto decryptOaepSha256(const KeyReference &reference, std::span<const std::uint8_t> ciphertext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
} // namespace SZX::KEYSTORE
