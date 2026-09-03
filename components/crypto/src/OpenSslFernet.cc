#include <openssl/evp.h>
#include "OpenSslInternal.hpp"
#include "StringHelper.hpp"
#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Fernet.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
#include "TimeHelper.hpp"
using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
using namespace internal;
namespace
{
auto crypt(bool enc, std::span<const std::uint8_t> key, std::span<const std::uint8_t> iv, std::span<const std::uint8_t> input)
    -> Result<std::vector<std::uint8_t>>
{
    if (!validAesCbcInput(key, iv, input.size()))
        return fail<std::vector<std::uint8_t>>(1601, "AES-CBC parameters invalid.");
    CipherCtx c(EVP_CIPHER_CTX_new());
    std::vector<std::uint8_t> out(input.size() + 16);
    int n = 0, total = 0;
    std::uint8_t dummy = 0;
    const auto *in     = input.empty() ? &dummy : input.data();
    if (!c || EVP_CipherInit_ex(c.get(), EVP_aes_128_cbc(), nullptr, key.data(), iv.data(), enc ? 1 : 0) != 1 ||
        EVP_CipherUpdate(c.get(), out.data(), &n, in, static_cast<int>(input.size())) != 1)
        return fail<std::vector<std::uint8_t>>(1601, "AES-CBC operation failed.");
    total = n;
    if (EVP_CipherFinal_ex(c.get(), out.data() + total, &n) != 1)
        return fail<std::vector<std::uint8_t>>(1601, "AES-CBC finalization failed.");
    out.resize(total + n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
} // namespace
auto encryptFernetImpl(std::string_view plain, std::string_view keyText) -> Result<std::string>
{
    auto k = decodeBase64(keyText, Base64Alphabet::UrlSafe);
    if (!k.ok || k.value.size() != 32)
        return fail<std::string>(1601, "invalid Fernet key.");
    auto iv = randomBytes(16);
    if (!iv.ok)
        return Result<std::string>::Failure(iv.error, iv.message);
    auto p = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(plain.data()), plain.size());
    auto c = crypt(true, std::span<const std::uint8_t>(k.value).subspan(16), iv.value, p);
    if (!c.ok)
        return Result<std::string>::Failure(c.error, c.message);
    std::vector<std::uint8_t> t{0x80};
    auto ts = static_cast<std::uint64_t>(TimeHelper::NowTimestamp() / 1000);
    for (int s = 56; s >= 0; s -= 8)
        t.push_back(static_cast<std::uint8_t>(ts >> s));
    t.insert(t.end(), iv.value.begin(), iv.value.end());
    t.insert(t.end(), c.value.begin(), c.value.end());
    auto m = hmac(HashAlgorithm::Sha256, std::span<const std::uint8_t>(k.value).first(16), t);
    if (!m.ok)
        return Result<std::string>::Failure(m.error, m.message);
    t.insert(t.end(), m.value.begin(), m.value.end());
    auto encoded = encodeBase64(t, Base64Alphabet::UrlSafe);
    return encoded.ok ? Result<std::string>::Success(std::move(encoded.value)) :
                        Result<std::string>::Failure(encoded.error, encoded.message);
}
auto decryptFernetImpl(std::string_view tokenText, std::string_view keyText) -> Result<std::vector<std::uint8_t>>
{
    auto k = decodeBase64(keyText, Base64Alphabet::UrlSafe);
    auto t = decodeBase64(StringHelper::Strip(std::string(tokenText)), Base64Alphabet::UrlSafe);
    if (!k.ok || !t.ok || k.value.size() != 32 || t.value.size() < 57 || t.value[0] != 0x80)
        return fail<std::vector<std::uint8_t>>(1601, "invalid Fernet token.");
    auto n = t.value.size() - 32;
    if (n < 25 || (n - 25) == 0 || (n - 25) % 16 != 0)
        return fail<std::vector<std::uint8_t>>(1601, "invalid Fernet ciphertext length.");
    auto m = hmac(HashAlgorithm::Sha256, std::span<const std::uint8_t>(k.value).first(16),
                  std::span<const std::uint8_t>(t.value).first(n));
    if (!m.ok || !internal::constantTimeEqual(m.value, std::span<const std::uint8_t>(t.value).subspan(n)))
        return fail<std::vector<std::uint8_t>>(1601, "Fernet authentication failed.");
    return crypt(false, std::span<const std::uint8_t>(k.value).subspan(16), std::span<const std::uint8_t>(t.value).subspan(9, 16),
                 std::span<const std::uint8_t>(t.value).subspan(25, n - 25));
}
auto encryptFernet(std::string_view plain, std::string_view keyText) -> Result<std::string>
{
    return invokeBoundary([&] { return encryptFernetImpl(plain, keyText); });
}
auto decryptFernet(std::string_view tokenText, std::string_view keyText) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return decryptFernetImpl(tokenText, keyText); });
}
} // namespace SZX::CRYPTO
