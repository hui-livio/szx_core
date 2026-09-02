/**
 * @file Kdf.h
 * @brief PBKDF2-HMAC-SHA256 密钥派生接口。
 */
#pragma once

#include "Result.hpp"
#include "SzxCrypto/CryptoExport.h"

#include <cstdint>
#include <span>
#include <vector>

namespace SZX::CRYPTO
{

enum class KdfError : int
{
    InvalidArgument = 1730,
    ProviderFailure = 1731,
    Unsupported = 1732,
};

/** 使用 PBKDF2-HMAC-SHA256 从口令派生指定长度的密钥。 */
[[nodiscard]] SZX_CRYPTO_API auto derivePbkdf2Sha256(std::span<const std::uint8_t> password,
                                                     std::span<const std::uint8_t> salt,
                                                     std::uint32_t iterations,
                                                     std::size_t keyLength)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

} // namespace SZX::CRYPTO
