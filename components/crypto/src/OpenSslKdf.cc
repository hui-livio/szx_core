#include <climits>
#include <openssl/evp.h>
#include "OpenSslInternal.hpp"
#include "SzxCrypto/Kdf.h"
using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
using namespace internal;
auto derivePbkdf2Sha256Impl(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, std::uint32_t iterations,
                            std::size_t length) -> Result<std::vector<std::uint8_t>>
{
    if (salt.empty() || !iterations || !length || length > 1024 * 1024 || !fitsInt(length) ||
        !fitsIntPair(password.size(), salt.size()))
        return fail<std::vector<std::uint8_t>>(1730, "PBKDF2 parameters invalid.");
    std::vector<std::uint8_t> out(length);
    if (PKCS5_PBKDF2_HMAC(reinterpret_cast<const char *>(password.data()), static_cast<int>(password.size()), salt.data(),
                          static_cast<int>(salt.size()), iterations, EVP_sha256(), static_cast<int>(length), out.data()) != 1)
        return fail<std::vector<std::uint8_t>>(1731, "PBKDF2 failed.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto derivePbkdf2Sha256(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, std::uint32_t iterations,
                        std::size_t length) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return derivePbkdf2Sha256Impl(password, salt, iterations, length); });
}
} // namespace SZX::CRYPTO
