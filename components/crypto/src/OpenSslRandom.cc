#include <climits>
#include <openssl/rand.h>
#include "OpenSslInternal.hpp"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
using namespace internal;
auto randomBytes(std::size_t size) -> Result<std::vector<std::uint8_t>>
{
    if (!fitsInt(size))
        return fail<std::vector<std::uint8_t>>(1720, "random size invalid.");
    std::vector<std::uint8_t> out(size);
    if (size && RAND_bytes(out.data(), static_cast<int>(size)) != 1)
        return fail<std::vector<std::uint8_t>>(1720, "random generation failed.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto constantTimeEqual(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) noexcept -> bool
{
    return internal::constantTimeEqual(a, b);
}
auto sha256KeyId(std::span<const std::uint8_t> k) -> Result<std::string>
{
    return digestHex(HashAlgorithm::Sha256, k);
}
} // namespace SZX::CRYPTO
