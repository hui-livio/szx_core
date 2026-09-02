/**
 * @file Hash.h
 * @brief Hash 和 HMAC 接口。
 *
 * 提供 SHA 摘要和带密钥 HMAC 操作。接口使用项目 Result 返回 provider
 * 错误，不向调用方暴露第三方异常。
 */
#pragma once

#include "SzxCrypto/CryptoExport.h"
#include "Result.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::CRYPTO
{

/** 摘要和 HMAC 支持的哈希算法。 */
enum class HashAlgorithm
{
    Sha1,   ///< SHA-1 摘要算法。
    Sha256, ///< SHA-256 摘要算法。
    Sha512  ///< SHA-512 摘要算法。
};

/** 计算字节数据的二进制摘要。 */
[[nodiscard]] SZX_CRYPTO_API auto digest(HashAlgorithm algorithm, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 按原始字符串字节计算二进制摘要，不进行字符集转换。 */
[[nodiscard]] SZX_CRYPTO_API auto digest(HashAlgorithm algorithm, std::string_view data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 计算字节数据的小写十六进制摘要。 */
[[nodiscard]] SZX_CRYPTO_API auto digestHex(HashAlgorithm algorithm, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::string>;

/** 按原始字符串字节计算小写十六进制摘要。 */
[[nodiscard]] SZX_CRYPTO_API auto digestHex(HashAlgorithm algorithm, std::string_view data)
    -> SZX::COMMON::Result<std::string>;

/** 计算字节数据的二进制 HMAC。 */
[[nodiscard]] SZX_CRYPTO_API auto hmac(HashAlgorithm algorithm, std::span<const std::uint8_t> key,
                                       std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 按原始字符串字节计算二进制 HMAC。 */
[[nodiscard]] SZX_CRYPTO_API auto hmac(HashAlgorithm algorithm, std::string_view key, std::string_view data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 计算字节数据的小写十六进制 HMAC。 */
[[nodiscard]] SZX_CRYPTO_API auto hmacHex(HashAlgorithm algorithm, std::span<const std::uint8_t> key,
                                          std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::string>;

/** 按原始字符串字节计算小写十六进制 HMAC。 */
[[nodiscard]] SZX_CRYPTO_API auto hmacHex(HashAlgorithm algorithm, std::string_view key, std::string_view data)
    -> SZX::COMMON::Result<std::string>;

} // namespace SZX::CRYPTO
