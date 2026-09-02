#include "SzxCryptoFile/BufferFile.h"
#include <array>
#include <cstring>
#include <utility>

namespace SZX::CRYPTO_FILE
{
const std::array<std::uint8_t, 5> kAad{'S', 'Z', 'X', 'E', 1};

auto encryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> plaintext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    auto e = SZX::CRYPTO::aes256GcmEncrypt(key, plaintext, kAad);
    if (!e.ok)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(e.error, e.message);
    std::vector<std::uint8_t> out{'S', 'Z', 'X', 'E', 1, 0, 0, 0};
    out.insert(out.end(), e.value.nonce.begin(), e.value.nonce.end());
    std::uint32_t n = (std::uint32_t)e.value.ciphertext.size();
    for (int i = 0; i < 4; ++i)
        out.push_back((n >> (8 * i)) & 255);
    out.insert(out.end(), e.value.ciphertext.begin(), e.value.ciphertext.end());
    out.insert(out.end(), e.value.tag.begin(), e.value.tag.end());
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out));
}

auto decryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> c)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    if (c.size() < 40 || std::memcmp(c.data(), "SZXE", 4) != 0 || c[4] != 1)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1900, "SZXE 容器头无效。");
    auto p          = 20;
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i)
        n |= (std::uint32_t)c[p++] << (8 * i);
    if (n > c.size() - p - 16 || c.size() != p + n + 16)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1900, "SZXE 容器长度无效。");
    return SZX::CRYPTO::aes256GcmDecrypt(key, c.subspan(8, 12), c.subspan(p, n), c.subspan(p + n, 16), kAad);
}
} // namespace SZX::CRYPTO_FILE
