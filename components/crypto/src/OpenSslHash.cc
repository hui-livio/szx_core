#include <climits>
#include <openssl/hmac.h>
#include <vector>
#include "OpenSslInternal.hpp"
#include "SzxCrypto/Hash.h"
using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
using namespace internal;
auto digest(HashAlgorithm a, std::span<const std::uint8_t> data) -> Result<std::vector<std::uint8_t>>
{
    if (!md(a) || !fitsInt(data.size()))
        return fail<std::vector<std::uint8_t>>(1600, "digest parameters invalid.");
    MdCtx ctx(EVP_MD_CTX_new());
    unsigned int n = 0;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(EVP_MD_size(md(a))));
    std::uint8_t dummy = 0;
    if (!ctx || EVP_DigestInit_ex(ctx.get(), md(a), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), data.empty() ? &dummy : data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), out.data(), &n) != 1)
        return fail<std::vector<std::uint8_t>>(1603, "digest operation failed.");
    out.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto digest(HashAlgorithm a, std::string_view s) -> Result<std::vector<std::uint8_t>>
{
    return digest(a, {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()});
}
auto digestHex(HashAlgorithm a, std::span<const std::uint8_t> d) -> Result<std::string>
{
    auto r = digest(a, d);
    return r.ok ? Result<std::string>::Success(hex(r.value)) : Result<std::string>::Failure(r.error, r.message);
}
auto digestHex(HashAlgorithm a, std::string_view s) -> Result<std::string>
{
    return digestHex(a, {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()});
}
auto hmac(HashAlgorithm a, std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> Result<std::vector<std::uint8_t>>
{
    if (!md(a) || !fitsIntPair(key.size(), data.size()))
        return fail<std::vector<std::uint8_t>>(1600, "HMAC parameters invalid.");
    unsigned int n = 0;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(EVP_MD_size(md(a))));
    std::uint8_t dummy = 0;
    const auto *kp     = key.empty() ? &dummy : key.data();
    const auto *dp     = data.empty() ? &dummy : data.data();
    if (!HMAC(md(a), kp, static_cast<int>(key.size()), dp, data.size(), out.data(), &n))
        return fail<std::vector<std::uint8_t>>(1603, "HMAC operation failed.");
    out.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto hmac(HashAlgorithm a, std::string_view k, std::string_view d) -> Result<std::vector<std::uint8_t>>
{
    return hmac(a, {reinterpret_cast<const std::uint8_t *>(k.data()), k.size()},
                {reinterpret_cast<const std::uint8_t *>(d.data()), d.size()});
}
auto hmacHex(HashAlgorithm a, std::span<const std::uint8_t> k, std::span<const std::uint8_t> d) -> Result<std::string>
{
    auto r = hmac(a, k, d);
    return r.ok ? Result<std::string>::Success(hex(r.value)) : Result<std::string>::Failure(r.error, r.message);
}
auto hmacHex(HashAlgorithm a, std::string_view k, std::string_view d) -> Result<std::string>
{
    return hmacHex(a, {reinterpret_cast<const std::uint8_t *>(k.data()), k.size()},
                   {reinterpret_cast<const std::uint8_t *>(d.data()), d.size()});
}
} // namespace SZX::CRYPTO
