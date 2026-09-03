#include <cstring>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include "OpenSslInternal.hpp"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
#include "SzxCrypto/Rsa.h"
using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
using namespace internal;
namespace
{
auto derPublic(std::span<const std::uint8_t> der) -> Pkey
{
    if (der.empty() || !fitsInt(der.size()))
        return {};
    const unsigned char *p = der.data(), *end = p + der.size();
    if (auto *k = d2i_PUBKEY(nullptr, &p, static_cast<long>(der.size())); k && p == end)
        return Pkey(k);
    if (der.size() < 24)
        return {};
    std::uint32_t magic = 0, bits = 0, ce = 0, cm = 0;
    std::memcpy(&magic, der.data(), 4);
    std::memcpy(&bits, der.data() + 4, 4);
    std::memcpy(&ce, der.data() + 8, 4);
    std::memcpy(&cm, der.data() + 12, 4);
    if (magic != 0x31415352u || !bits || !ce || !cm || 24ull + ce + cm > der.size())
        return {};
    std::vector<unsigned char> e(der.begin() + 24, der.begin() + 24 + ce), n(der.begin() + 24 + ce, der.begin() + 24 + ce + cm);
    std::reverse(e.begin(), e.end());
    std::reverse(n.begin(), n.end());
    PkeyCtx c(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
    BIGNUM *bnN       = BN_bin2bn(n.data(), static_cast<int>(n.size()), nullptr),
           *bnE       = BN_bin2bn(e.data(), static_cast<int>(e.size()), nullptr);
    OSSL_PARAM_BLD *b = OSSL_PARAM_BLD_new();
    if (!c || !b || !bnN || !bnE || EVP_PKEY_fromdata_init(c.get()) != 1 ||
        OSSL_PARAM_BLD_push_BN(b, OSSL_PKEY_PARAM_RSA_N, bnN) != 1 || OSSL_PARAM_BLD_push_BN(b, OSSL_PKEY_PARAM_RSA_E, bnE) != 1)
    {
        BN_free(bnN);
        BN_free(bnE);
        OSSL_PARAM_BLD_free(b);
        return {};
    }
    OSSL_PARAM *ps = OSSL_PARAM_BLD_to_param(b);
    EVP_PKEY *raw  = nullptr;
    int ok         = ps && EVP_PKEY_fromdata(c.get(), &raw, EVP_PKEY_PUBLIC_KEY, ps) == 1;
    OSSL_PARAM_free(ps);
    OSSL_PARAM_BLD_free(b);
    BN_free(bnN);
    BN_free(bnE);
    return ok ? Pkey(raw) : Pkey{};
}
auto derPrivate(std::span<const std::uint8_t> der) -> Pkey
{
    if (der.empty() || !fitsInt(der.size()))
        return {};
    const unsigned char *p = der.data(), *end = p + der.size();
    auto *k = d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(der.size()));
    return k && p == end ? Pkey(k) : Pkey{};
}
auto serializePublic(EVP_PKEY *k) -> Result<std::vector<std::uint8_t>>
{
    int n = i2d_PUBKEY(k, nullptr);
    if (n <= 0)
        return fail<std::vector<std::uint8_t>>(1711, "public key serialization failed.");
    std::vector<std::uint8_t> o(n);
    unsigned char *p = o.data();
    i2d_PUBKEY(k, &p);
    return Result<std::vector<std::uint8_t>>::Success(std::move(o));
}
auto serializePrivate(EVP_PKEY *k) -> Result<std::vector<std::uint8_t>>
{
    int n = i2d_PrivateKey(k, nullptr);
    if (n <= 0)
        return fail<std::vector<std::uint8_t>>(1711, "private key serialization failed.");
    std::vector<std::uint8_t> o(n);
    unsigned char *p = o.data();
    i2d_PrivateKey(k, &p);
    return Result<std::vector<std::uint8_t>>::Success(std::move(o));
}
auto rsaCrypt(bool enc, std::span<const std::uint8_t> key, std::span<const std::uint8_t> in) -> Result<std::vector<std::uint8_t>>
{
    if (!validRsaInput(key, in))
        return fail<std::vector<std::uint8_t>>(1710, "RSA parameters invalid.");
    Pkey p = enc ? derPublic(key) : derPrivate(key);
    if (!p)
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA key.");
    PkeyCtx c(EVP_PKEY_CTX_new(p.get(), nullptr));
    if (!c || (enc ? EVP_PKEY_encrypt_init(c.get()) : EVP_PKEY_decrypt_init(c.get())) != 1 ||
        EVP_PKEY_CTX_set_rsa_padding(c.get(), RSA_PKCS1_OAEP_PADDING) != 1 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(c.get(), EVP_sha256()) != 1 || EVP_PKEY_CTX_set_rsa_mgf1_md(c.get(), EVP_sha256()) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA OAEP initialization failed.");
    size_t n = 0;
    if ((enc ? EVP_PKEY_encrypt(c.get(), nullptr, &n, in.data(), in.size()) :
               EVP_PKEY_decrypt(c.get(), nullptr, &n, in.data(), in.size())) != 1)
        return fail<std::vector<std::uint8_t>>(1712, "RSA OAEP operation failed.");
    std::vector<std::uint8_t> o(n);
    if ((enc ? EVP_PKEY_encrypt(c.get(), o.data(), &n, in.data(), in.size()) :
               EVP_PKEY_decrypt(c.get(), o.data(), &n, in.data(), in.size())) != 1)
        return fail<std::vector<std::uint8_t>>(1712, "RSA OAEP operation failed.");
    o.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(o));
}
} // namespace
auto generateRsa3072KeyPair() -> Result<RsaKeyPair>
{
    PkeyCtx c(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    EVP_PKEY *raw = nullptr;
    if (!c || EVP_PKEY_keygen_init(c.get()) != 1 || EVP_PKEY_CTX_set_rsa_keygen_bits(c.get(), 3072) != 1 ||
        EVP_PKEY_keygen(c.get(), &raw) != 1)
        return fail<RsaKeyPair>(1711, "RSA key generation failed.");
    Pkey p(raw);
    auto pub = serializePublic(p.get()), priv = serializePrivate(p.get());
    if (!pub.ok || !priv.ok)
        return fail<RsaKeyPair>(1711, "RSA key serialization failed.");
    return Result<RsaKeyPair>::Success({std::move(pub.value), std::move(priv.value)});
}
auto rsaPublicKeyToDer(std::span<const std::uint8_t> k) -> Result<std::vector<std::uint8_t>>
{
    auto p = derPublic(k);
    return p ? serializePublic(p.get()) : fail<std::vector<std::uint8_t>>(1710, "invalid RSA public key.");
}
auto rsaPublicKeyFromDer(std::span<const std::uint8_t> k) -> Result<std::vector<std::uint8_t>>
{
    return rsaPublicKeyToDer(k);
}
auto rsaPublicKeyId(std::span<const std::uint8_t> k) -> Result<std::string>
{
    auto d = rsaPublicKeyToDer(k);
    return d.ok ? sha256KeyId(d.value) : Result<std::string>::Failure(d.error, d.message);
}
auto rsaPublicKeyIdFromDer(std::span<const std::uint8_t> k) -> Result<std::string>
{
    return rsaPublicKeyId(k);
}
auto rsaOaepSha256Encrypt(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d) -> Result<std::vector<std::uint8_t>>
{
    return rsaCrypt(true, k, d);
}
auto rsaOaepSha256Decrypt(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d) -> Result<std::vector<std::uint8_t>>
{
    return rsaCrypt(false, k, d);
}
auto rsaPssSha256Sign(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d) -> Result<std::vector<std::uint8_t>>
{
    Pkey p = derPrivate(k);
    if (!p)
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA private key.");
    auto h = digest(HashAlgorithm::Sha256, d);
    PkeyCtx c(EVP_PKEY_CTX_new(p.get(), nullptr));
    if (!h.ok || !c || EVP_PKEY_sign_init(c.get()) != 1 || EVP_PKEY_CTX_set_rsa_padding(c.get(), RSA_PKCS1_PSS_PADDING) != 1 ||
        EVP_PKEY_CTX_set_signature_md(c.get(), EVP_sha256()) != 1 || EVP_PKEY_CTX_set_rsa_pss_saltlen(c.get(), 32) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA signing failed.");
    size_t n = 0;
    if (EVP_PKEY_sign(c.get(), nullptr, &n, h.value.data(), h.value.size()) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA signing failed.");
    std::vector<std::uint8_t> o(n);
    if (EVP_PKEY_sign(c.get(), o.data(), &n, h.value.data(), h.value.size()) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA signing failed.");
    o.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(o));
}
auto rsaPssSha256Verify(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d, std::span<const std::uint8_t> s)
    -> Result<bool>
{
    auto p = derPublic(k);
    auto h = digest(HashAlgorithm::Sha256, d);
    PkeyCtx c(p ? EVP_PKEY_CTX_new(p.get(), nullptr) : nullptr);
    if (!p || !h.ok || !c || EVP_PKEY_verify_init(c.get()) != 1 ||
        EVP_PKEY_CTX_set_rsa_padding(c.get(), RSA_PKCS1_PSS_PADDING) != 1 ||
        EVP_PKEY_CTX_set_signature_md(c.get(), EVP_sha256()) != 1 || EVP_PKEY_CTX_set_rsa_pss_saltlen(c.get(), 32) != 1)
        return fail<bool>(1711, "RSA verification failed.");
    return Result<bool>::Success(EVP_PKEY_verify(c.get(), s.data(), s.size(), h.value.data(), h.value.size()) == 1);
}
} // namespace SZX::CRYPTO
