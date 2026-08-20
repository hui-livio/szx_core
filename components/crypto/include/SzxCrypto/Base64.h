/**
 * @file Base64.h
 * @brief Base64 and URL-safe Base64 helpers.
 *
 * Provides RFC 4648 style Base64 encoding and decoding for text and binary
 * buffers. APIs return project Result values for decode failures instead of
 * throwing exceptions.
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
 * @brief Selects the Base64 alphabet used by encode and decode operations.
 */
enum class Base64Alphabet
{
    Standard, ///< RFC 4648 standard alphabet using '+' and '/'.
    UrlSafe   ///< RFC 4648 URL-safe alphabet using '-' and '_'.
};

/**
 * @brief Encodes binary data to Base64 text.
 *
 * @param[in] input Binary input bytes to encode.
 * @param[in] alphabet Alphabet variant used for output characters.
 * @param[in] padding Whether '=' padding characters are emitted.
 * @return Encoded Base64 string.
 *
 * @note This function is deterministic and does not fail.
 */
[[nodiscard]] SZX_CRYPTO_API auto encodeBase64(std::span<const std::uint8_t> input,
                                               Base64Alphabet alphabet = Base64Alphabet::Standard, bool padding = true)
    -> std::string;

/**
 * @brief Encodes string data to Base64 text.
 *
 * The string bytes are encoded as-is; no character set conversion is performed.
 *
 * @param[in] input String bytes to encode.
 * @param[in] alphabet Alphabet variant used for output characters.
 * @param[in] padding Whether '=' padding characters are emitted.
 * @return Encoded Base64 string.
 *
 * @note This function is deterministic and does not fail.
 */
[[nodiscard]] SZX_CRYPTO_API auto encodeBase64(std::string_view input, Base64Alphabet alphabet = Base64Alphabet::Standard,
                                               bool padding = true) -> std::string;

/**
 * @brief Decodes Base64 text to binary bytes.
 *
 * Whitespace characters are ignored. Missing padding is accepted when the
 * remaining Base64 input length is otherwise valid for the selected alphabet.
 *
 * @param[in] input Base64 text to decode.
 * @param[in] alphabet Alphabet variant expected in the input text.
 * @return Success with decoded bytes, or failure when the input is malformed.
 */
[[nodiscard]] SZX_CRYPTO_API auto decodeBase64(std::string_view input, Base64Alphabet alphabet = Base64Alphabet::Standard)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

} // namespace SZX::CRYPTO
