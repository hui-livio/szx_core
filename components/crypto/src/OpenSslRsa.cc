#include <openssl/pem.h>
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
constexpr std::size_t kRsa3072ModulusBytes       = 384;
constexpr std::size_t kRsaOaepSha256MaxPlaintext = kRsa3072ModulusBytes - 2 * 32 - 2;
auto derPublic(std::span<const std::uint8_t> der) -> Pkey
{
    if (der.empty() || !fitsInt(der.size()))
        return {};
    const unsigned char *p = der.data(), *end = p + der.size();
    auto *k = d2i_PUBKEY(nullptr, &p, static_cast<long>(der.size()));
    if (k && p == end && EVP_PKEY_is_a(k, "RSA") == 1)
        return Pkey(k);
    EVP_PKEY_free(k);
    return {};
}
auto derPrivate(std::span<const std::uint8_t> der) -> Pkey
{
    if (der.empty() || !fitsInt(der.size()))
        return {};
    const unsigned char *p = der.data(), *end = p + der.size();
    auto *k = d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(der.size()));
    if (k && p == end && EVP_PKEY_is_a(k, "RSA") == 1)
        return Pkey(k);
    EVP_PKEY_free(k);
    return {};
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
auto rsaCryptImpl(bool enc, std::span<const std::uint8_t> key, std::span<const std::uint8_t> in)
    -> Result<std::vector<std::uint8_t>>
{
    if (!validRsaInput(key, in) || (enc ? in.size() > kRsaOaepSha256MaxPlaintext : in.size() != kRsa3072ModulusBytes))
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
auto rsaCrypt(bool enc, std::span<const std::uint8_t> key, std::span<const std::uint8_t> in) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return rsaCryptImpl(enc, key, in); });
}
} // namespace
auto generateRsa3072KeyPairImpl() -> Result<RsaKeyPair>
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
auto generateRsa3072KeyPair() -> Result<RsaKeyPair>
{
    return invokeBoundary([] { return generateRsa3072KeyPairImpl(); });
}
auto rsaPublicKeyToDer(std::span<const std::uint8_t> k) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary(
        [&]
        {
            auto p = derPublic(k);
            return p ? serializePublic(p.get()) : fail<std::vector<std::uint8_t>>(1710, "invalid RSA public key.");
        });
}
auto rsaPublicKeyFromDer(std::span<const std::uint8_t> k) -> Result<std::vector<std::uint8_t>>
{
    return rsaPublicKeyToDer(k);
}
auto rsaPublicKeyFromPemImpl(std::span<const std::uint8_t> pem) -> Result<std::vector<std::uint8_t>>
{
    if (pem.empty() || !fitsInt(pem.size()))
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA public key PEM.");
    Bio bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    Pkey key(bio ? PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr) : nullptr);
    if (!key || EVP_PKEY_is_a(key.get(), "RSA") != 1)
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA public key PEM.");
    return serializePublic(key.get());
}
auto rsaPublicKeyFromPem(std::span<const std::uint8_t> pem) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return rsaPublicKeyFromPemImpl(pem); });
}
auto rsaPrivateKeyFromPemImpl(std::span<const std::uint8_t> pem) -> Result<std::vector<std::uint8_t>>
{
    if (pem.empty() || !fitsInt(pem.size()))
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA private key PEM.");
    Bio bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    Pkey key(bio ? PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr) : nullptr);
    if (!key || EVP_PKEY_is_a(key.get(), "RSA") != 1)
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA private key PEM.");
    return serializePrivate(key.get());
}
auto rsaPrivateKeyFromPem(std::span<const std::uint8_t> pem) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return rsaPrivateKeyFromPemImpl(pem); });
}
auto rsaPublicKeyId(std::span<const std::uint8_t> k) -> Result<std::string>
{
    return invokeBoundary(
        [&]
        {
            auto d = rsaPublicKeyToDer(k);
            return d.ok ? sha256KeyId(d.value) : Result<std::string>::Failure(d.error, d.message);
        });
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
auto rsaPssSha256SignImpl(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d) -> Result<std::vector<std::uint8_t>>
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
auto rsaPssSha256Sign(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d) -> Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return rsaPssSha256SignImpl(k, d); });
}
auto rsaPssSha256VerifyImpl(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d, std::span<const std::uint8_t> s)
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
auto rsaPssSha256Verify(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d, std::span<const std::uint8_t> s)
    -> Result<bool>
{
    return invokeBoundary([&] { return rsaPssSha256VerifyImpl(k, d, s); });
}
} // namespace SZX::CRYPTO
