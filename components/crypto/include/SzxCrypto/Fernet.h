/**
 * @file Fernet.h
 * @brief Python cryptography.fernet compatible decrypt helpers.
 *
 * Provides Fernet token encryption and decryption compatible with Python's
 * cryptography.fernet package for AES-128-CBC and HMAC-SHA256 tokens.
 */

#pragma once

#include "SzxCrypto/CryptoExport.h"

#include "Result.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace SZX::CRYPTO
{

/**
 * @brief Decrypts a Fernet token to plaintext bytes.
 *
 * The key must be a URL-safe Base64 encoded 32-byte Fernet key. The token is
 * authenticated before decryption, so tampered tokens fail without returning
 * plaintext.
 *
 * @param[in] encryptedToken URL-safe Base64 Fernet token text.
 * @param[in] urlSafeBase64Key URL-safe Base64 encoded 32-byte Fernet key.
 * @return Success with decrypted bytes, or failure for malformed keys, malformed
 *         tokens, authentication failures, or crypto provider errors.
 *
 * @note Token TTL and timestamp freshness are not enforced by this helper.
 */
[[nodiscard]] SZX_CRYPTO_API auto decryptFernet(std::string_view encryptedToken, std::string_view urlSafeBase64Key)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief Encrypts plaintext bytes into a Fernet token.
 *
 * The key must be a URL-safe Base64 encoded 32-byte Fernet key. The generated
 * token includes the current system timestamp and a random initialization vector.
 *
 * @param[in] plainData Plaintext bytes to encrypt.
 * @param[in] urlSafeBase64Key URL-safe Base64 encoded 32-byte Fernet key.
 * @return Success with URL-safe Base64 Fernet token text, or failure for malformed
 *         keys or crypto provider errors.
 */
[[nodiscard]] SZX_CRYPTO_API auto encryptFernet(std::string_view plainData, std::string_view urlSafeBase64Key)
    -> SZX::COMMON::Result<std::string>;

} // namespace SZX::CRYPTO
