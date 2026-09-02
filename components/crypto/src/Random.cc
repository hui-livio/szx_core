#include "SzxCrypto/Random.h"

#include "SzxCrypto/Hash.h"

#include <limits>
#include <utility>

#if defined(_WIN32)
    #include <Windows.h>
    #include <bcrypt.h>
#endif

namespace SZX::CRYPTO
{
namespace
{
constexpr int kRandomError = 1720;
}

auto randomBytes(std::size_t size) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    if (size == 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success({});
    if (size > static_cast<std::size_t>(std::numeric_limits<ULONG>::max()))
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(kRandomError, "随机字节长度超出平台限制。");
    std::vector<std::uint8_t> result(size);
    if (BCryptGenRandom(nullptr, result.data(), static_cast<ULONG>(result.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(kRandomError, "系统随机源生成失败。");
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(result));
#else
    (void)size;
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(kRandomError, "当前平台未提供密码学随机源。");
#endif
}

auto constantTimeEqual(std::span<const std::uint8_t> left, std::span<const std::uint8_t> right) noexcept -> bool
{
    std::uint8_t difference = static_cast<std::uint8_t>(left.size() ^ right.size());
    const auto count        = left.size() < right.size() ? left.size() : right.size();
    for (std::size_t i = 0; i < count; ++i)
        difference |= static_cast<std::uint8_t>(left[i] ^ right[i]);
    return difference == 0 && left.size() == right.size();
}

auto sha256KeyId(std::span<const std::uint8_t> key) -> SZX::COMMON::Result<std::string>
{
    return digestHex(HashAlgorithm::Sha256, key);
}

} // namespace SZX::CRYPTO
