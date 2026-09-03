/**
 * @file Base64.h
 * @brief Base64 与 URL-safe Base64 编解码接口。
 *
 * 提供符合 RFC 4648 的文本和二进制缓冲区编解码。解码失败通过项目
 * Result 返回，不向调用方抛出异常。
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
 * @brief 选择编码和解码所使用的 Base64 字母表。
 */
enum class Base64Alphabet
{
    Standard, ///< RFC 4648 标准字母表，使用 '+' 和 '/'.
    UrlSafe   ///< RFC 4648 URL 安全字母表，使用 '-' 和 '_'.
};

/** Base64 操作返回的稳定错误码。 */
enum class Base64Error : int
{
    InvalidArgument = 1602,
    EncodingFailed  = 1603,
};

/**
 * @brief 将二进制数据编码为 Base64 文本。
 *
 * @param[in] input 待编码的输入字节。
 * @param[in] alphabet 输出所使用的字母表。
 * @param[in] padding 是否输出 '=' 填充字符。
 * @return 成功时返回编码后的 Base64 字符串；分配失败时返回错误。
 */
[[nodiscard]] SZX_CRYPTO_API auto encodeBase64(std::span<const std::uint8_t> input,
                                               Base64Alphabet alphabet = Base64Alphabet::Standard, bool padding = true)
    -> SZX::COMMON::Result<std::string>;

/**
 * @brief 将字符串字节编码为 Base64 文本。
 *
 * 字符串按原始字节编码，不进行字符集转换。
 *
 * @param[in] input 待编码的字符串字节。
 * @param[in] alphabet 输出所使用的字母表。
 * @param[in] padding 是否输出 '=' 填充字符。
 * @return 成功时返回编码后的 Base64 字符串；分配失败时返回错误。
 */
[[nodiscard]] SZX_CRYPTO_API auto encodeBase64(std::string_view input, Base64Alphabet alphabet = Base64Alphabet::Standard,
                                               bool padding = true) -> SZX::COMMON::Result<std::string>;

/**
 * @brief 将 Base64 文本解码为二进制字节。
 *
 * 空白字符会被忽略。当剩余长度符合所选字母表规则时允许省略填充。
 *
 * @param[in] input 待解码的 Base64 文本。
 * @param[in] alphabet 输入文本所使用的字母表。
 * @return 成功时返回解码字节；输入格式错误时返回失败。
 */
[[nodiscard]] SZX_CRYPTO_API auto decodeBase64(std::string_view input, Base64Alphabet alphabet = Base64Alphabet::Standard)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

} // namespace SZX::CRYPTO
