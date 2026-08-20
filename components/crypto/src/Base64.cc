/**
 * @file Base64.cc
 * @brief Base64 helper implementation.
 */

#include "SzxCrypto/Base64.h"

#include <array>
#include <utility>

using namespace SZX::COMMON;

namespace SZX::CRYPTO
{
namespace
{
constexpr int kBase64Error = 1602;

[[nodiscard]] auto alphabetChars(Base64Alphabet alphabet) -> const char *
{
    return alphabet == Base64Alphabet::UrlSafe ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" :
                                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

[[nodiscard]] auto decodeTable(Base64Alphabet alphabet) -> std::array<unsigned char, 256>
{
    static constexpr auto kInvalid = 0xFFU;
    std::array<unsigned char, 256> table{};
    table.fill(static_cast<unsigned char>(kInvalid));
    for (unsigned char ch = 'A'; ch <= 'Z'; ++ch)
        table[ch] = static_cast<unsigned char>(ch - 'A');
    for (unsigned char ch = 'a'; ch <= 'z'; ++ch)
        table[ch] = static_cast<unsigned char>(26 + ch - 'a');
    for (unsigned char ch = '0'; ch <= '9'; ++ch)
        table[ch] = static_cast<unsigned char>(52 + ch - '0');
    if (alphabet == Base64Alphabet::UrlSafe)
    {
        table[static_cast<unsigned char>('-')] = 62;
        table[static_cast<unsigned char>('_')] = 63;
    }
    else
    {
        table[static_cast<unsigned char>('+')] = 62;
        table[static_cast<unsigned char>('/')] = 63;
    }
    return table;
}

} // namespace

auto encodeBase64(std::span<const std::uint8_t> input, Base64Alphabet alphabet, bool padding) -> std::string
{
    const auto *chars = alphabetChars(alphabet);
    std::string encoded;
    encoded.reserve(((input.size() + 2) / 3) * 4);
    for (std::size_t index = 0; index < input.size(); index += 3)
    {
        const auto remaining = input.size() - index;
        const auto b0        = input[index];
        const auto b1        = remaining > 1 ? input[index + 1] : 0;
        const auto b2        = remaining > 2 ? input[index + 2] : 0;
        encoded.push_back(chars[b0 >> 2]);
        encoded.push_back(chars[((b0 & 0x03U) << 4U) | (b1 >> 4U)]);
        if (remaining > 1)
            encoded.push_back(chars[((b1 & 0x0FU) << 2U) | (b2 >> 6U)]);
        else if (padding)
            encoded.push_back('=');
        if (remaining > 2)
            encoded.push_back(chars[b2 & 0x3FU]);
        else if (padding)
            encoded.push_back('=');
    }
    return encoded;
}

auto encodeBase64(std::string_view input, Base64Alphabet alphabet, bool padding) -> std::string
{
    return encodeBase64(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(input.data()), input.size()),
                        alphabet, padding);
}

auto decodeBase64(std::string_view input, Base64Alphabet alphabet) -> Result<std::vector<std::uint8_t>>
{
    static constexpr auto kInvalid = 0xFFU;
    const auto table               = decodeTable(alphabet);
    std::vector<std::uint8_t> decoded;
    decoded.reserve((input.size() / 4) * 3 + 3);
    std::uint32_t buffer     = 0;
    int bits                 = 0;
    bool paddingStarted      = false;
    std::size_t valueChars   = 0;
    std::size_t paddingChars = 0;
    for (const unsigned char ch : input)
    {
        if (ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ')
            continue;
        if (ch == '=')
        {
            paddingStarted = true;
            ++paddingChars;
            if (paddingChars > 2)
                return Result<std::vector<std::uint8_t>>::Failure(kBase64Error, "invalid base64 data.");
            continue;
        }
        if (paddingStarted || table[ch] == kInvalid)
            return Result<std::vector<std::uint8_t>>::Failure(kBase64Error, "invalid base64 data.");
        ++valueChars;
        buffer  = (buffer << 6U) | table[ch];
        bits   += 6;
        if (bits >= 8)
        {
            bits -= 8;
            decoded.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFFU));
        }
    }
    if (valueChars == 0 && paddingChars > 0)
        return Result<std::vector<std::uint8_t>>::Failure(kBase64Error, "invalid base64 data.");
    if ((valueChars % 4U) == 1U)
        return Result<std::vector<std::uint8_t>>::Failure(kBase64Error, "invalid base64 data.");
    if (paddingChars > 0 && ((valueChars + paddingChars) % 4U) != 0U)
        return Result<std::vector<std::uint8_t>>::Failure(kBase64Error, "invalid base64 data.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(decoded));
}

} // namespace SZX::CRYPTO
