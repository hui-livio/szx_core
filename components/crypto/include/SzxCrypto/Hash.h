/**
 * @file Hash.h
 * @brief Hash and HMAC helpers.
 *
 * Provides common digest and keyed-HMAC operations behind the project Result
 * type. Current implementations use Windows CNG/BCrypt when built on Windows.
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

/**
 * @brief Supported hash algorithms for digest and HMAC helpers.
 */
enum class HashAlgorithm
{
    Sha1,   ///< SHA-1 digest algorithm.
    Sha256, ///< SHA-256 digest algorithm.
    Sha512  ///< SHA-512 digest algorithm.
};

/**
 * @brief Computes a binary digest for byte data.
 *
 * @param[in] algorithm Hash algorithm to use.
 * @param[in] data Input bytes to hash.
 * @return Success with raw digest bytes, or failure if the crypto provider fails.
 */
[[nodiscard]] SZX_CRYPTO_API auto digest(HashAlgorithm algorithm, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief Computes a binary digest for string data.
 *
 * The string bytes are hashed as-is; no character set conversion is performed.
 *
 * @param[in] algorithm Hash algorithm to use.
 * @param[in] data Input string bytes to hash.
 * @return Success with raw digest bytes, or failure if the crypto provider fails.
 */
[[nodiscard]] SZX_CRYPTO_API auto digest(HashAlgorithm algorithm, std::string_view data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief Computes a lowercase hexadecimal digest for byte data.
 *
 * @param[in] algorithm Hash algorithm to use.
 * @param[in] data Input bytes to hash.
 * @return Success with lowercase hex digest text, or failure on provider error.
 */
[[nodiscard]] SZX_CRYPTO_API auto digestHex(HashAlgorithm algorithm, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::string>;

/**
 * @brief Computes a lowercase hexadecimal digest for string data.
 *
 * @param[in] algorithm Hash algorithm to use.
 * @param[in] data Input string bytes to hash.
 * @return Success with lowercase hex digest text, or failure on provider error.
 */
[[nodiscard]] SZX_CRYPTO_API auto digestHex(HashAlgorithm algorithm, std::string_view data) -> SZX::COMMON::Result<std::string>;

/**
 * @brief Computes a binary HMAC for byte data.
 *
 * @param[in] algorithm Hash algorithm used by HMAC.
 * @param[in] key HMAC key bytes.
 * @param[in] data Input bytes to authenticate.
 * @return Success with raw HMAC bytes, or failure if the crypto provider fails.
 */
[[nodiscard]] SZX_CRYPTO_API auto hmac(HashAlgorithm algorithm, std::span<const std::uint8_t> key,
                                       std::span<const std::uint8_t> data) -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief Computes a binary HMAC for string data.
 *
 * The key and data string bytes are used as-is; no character set conversion is
 * performed.
 *
 * @param[in] algorithm Hash algorithm used by HMAC.
 * @param[in] key HMAC key string bytes.
 * @param[in] data Input string bytes to authenticate.
 * @return Success with raw HMAC bytes, or failure if the crypto provider fails.
 */
[[nodiscard]] SZX_CRYPTO_API auto hmac(HashAlgorithm algorithm, std::string_view key, std::string_view data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief Computes a lowercase hexadecimal HMAC for byte data.
 *
 * @param[in] algorithm Hash algorithm used by HMAC.
 * @param[in] key HMAC key bytes.
 * @param[in] data Input bytes to authenticate.
 * @return Success with lowercase hex HMAC text, or failure on provider error.
 */
[[nodiscard]] SZX_CRYPTO_API auto hmacHex(HashAlgorithm algorithm, std::span<const std::uint8_t> key,
                                          std::span<const std::uint8_t> data) -> SZX::COMMON::Result<std::string>;

/**
 * @brief Computes a lowercase hexadecimal HMAC for string data.
 *
 * @param[in] algorithm Hash algorithm used by HMAC.
 * @param[in] key HMAC key string bytes.
 * @param[in] data Input string bytes to authenticate.
 * @return Success with lowercase hex HMAC text, or failure on provider error.
 */
[[nodiscard]] SZX_CRYPTO_API auto hmacHex(HashAlgorithm algorithm, std::string_view key, std::string_view data)
    -> SZX::COMMON::Result<std::string>;

} // namespace SZX::CRYPTO
