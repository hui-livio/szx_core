#include "SzxCrypto/Aead.h"
#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Fernet.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Kdf.h"
#include "SzxCrypto/Random.h"
#include "SzxCrypto/Rsa.h"

#include "StringHelper.hpp"
#include "TimeHelper.hpp"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
namespace
{
template <typename T, void (*Free)(T *)>
struct Deleter
{
    void operator()(T *p) const noexcept
    {
        if (p)
            Free(p);
    }
};
template <typename T, void (*Free)(T *)>
using Handle    = std::unique_ptr<T, Deleter<T, Free>>;
using MdCtx     = Handle<EVP_MD_CTX, EVP_MD_CTX_free>;
using CipherCtx = Handle<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free>;
using Pkey      = Handle<EVP_PKEY, EVP_PKEY_free>;
using PkeyCtx   = Handle<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;

template <typename T>
auto fail(int code, const char *message) -> Result<T>
{
    return Result<T>::Failure(code, message);
}
auto md(HashAlgorithm a) -> const EVP_MD *
{
    switch (a)
    {
    case HashAlgorithm::Sha1:
        return EVP_sha1();
    case HashAlgorithm::Sha256:
        return EVP_sha256();
    case HashAlgorithm::Sha512:
        return EVP_sha512();
    default:
        return nullptr;
    }
}
auto hex(std::span<const std::uint8_t> bytes) -> std::string
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto b : bytes)
        out << std::setw(2) << static_cast<unsigned>(b);
    return out.str();
}
auto derPublic(std::span<const std::uint8_t> der) -> Pkey
{
    const unsigned char *p = der.data();
    if (auto *key = d2i_PUBKEY(nullptr, &p, static_cast<long>(der.size())))
        return Pkey(key);
    if (der.size() < 24)
        return {};
    std::uint32_t magic = 0, bits = 0, cbExp = 0, cbMod = 0;
    std::memcpy(&magic, der.data(), 4);
    std::memcpy(&bits, der.data() + 4, 4);
    std::memcpy(&cbExp, der.data() + 8, 4);
    std::memcpy(&cbMod, der.data() + 12, 4);
    if (magic != 0x31415352u || bits == 0 || cbExp == 0 || cbMod == 0 || 24ull + cbExp + cbMod > der.size())
        return {};
    std::vector<unsigned char> e(der.begin() + 24, der.begin() + 24 + cbExp),
        n(der.begin() + 24 + cbExp, der.begin() + 24 + cbExp + cbMod);
    std::reverse(e.begin(), e.end());
    std::reverse(n.begin(), n.end());
    PkeyCtx ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
    BIGNUM *bnN             = BN_bin2bn(n.data(), static_cast<int>(n.size()), nullptr),
           *bnE             = BN_bin2bn(e.data(), static_cast<int>(e.size()), nullptr);
    OSSL_PARAM_BLD *builder = OSSL_PARAM_BLD_new();
    if (!ctx || !builder || !bnN || !bnE || EVP_PKEY_fromdata_init(ctx.get()) != 1 ||
        OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_N, bnN) != 1 ||
        OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_E, bnE) != 1)
    {
        BN_free(bnN);
        BN_free(bnE);
        OSSL_PARAM_BLD_free(builder);
        return {};
    }
    OSSL_PARAM *params = OSSL_PARAM_BLD_to_param(builder);
    EVP_PKEY *raw      = nullptr;
    const int ok       = params && EVP_PKEY_fromdata(ctx.get(), &raw, EVP_PKEY_PUBLIC_KEY, params) == 1;
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(builder);
    BN_free(bnN);
    BN_free(bnE);
    return ok ? Pkey(raw) : Pkey{};
}
auto derPrivate(std::span<const std::uint8_t> der) -> Pkey
{
    const unsigned char *p = der.data();
    return Pkey(d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(der.size())));
}
auto serializePublic(EVP_PKEY *key) -> Result<std::vector<std::uint8_t>>
{
    const int size = i2d_PUBKEY(key, nullptr);
    if (size <= 0)
        return fail<std::vector<std::uint8_t>>(1711, "public key serialization failed.");
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    unsigned char *p = out.data();
    i2d_PUBKEY(key, &p);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto serializePrivate(EVP_PKEY *key) -> Result<std::vector<std::uint8_t>>
{
    const int size = i2d_PrivateKey(key, nullptr);
    if (size <= 0)
        return fail<std::vector<std::uint8_t>>(1711, "private key serialization failed.");
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    unsigned char *p = out.data();
    i2d_PrivateKey(key, &p);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto crypt(bool encrypt, std::span<const std::uint8_t> key, std::span<const std::uint8_t> iv, std::span<const std::uint8_t> input)
    -> Result<std::vector<std::uint8_t>>
{
    CipherCtx ctx(EVP_CIPHER_CTX_new());
    if (!ctx || EVP_CipherInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, key.data(), iv.data(), encrypt ? 1 : 0) != 1)
        return fail<std::vector<std::uint8_t>>(1601, "AES-CBC initialization failed.");
    std::vector<std::uint8_t> out(input.size() + 16);
    int n = 0, total = 0;
    if (EVP_CipherUpdate(ctx.get(), out.data(), &n, input.data(), static_cast<int>(input.size())) != 1)
        return fail<std::vector<std::uint8_t>>(1601, "AES-CBC operation failed.");
    total = n;
    if (EVP_CipherFinal_ex(ctx.get(), out.data() + total, &n) != 1)
        return fail<std::vector<std::uint8_t>>(1601, "AES-CBC finalization failed.");
    out.resize(static_cast<std::size_t>(total + n));
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
} // namespace

auto digest(HashAlgorithm a, std::span<const std::uint8_t> data) -> Result<std::vector<std::uint8_t>>
{
    if (!md(a) || data.size() > static_cast<std::size_t>(INT_MAX))
        return fail<std::vector<std::uint8_t>>(1600, "digest parameters invalid.");
    MdCtx ctx(EVP_MD_CTX_new());
    unsigned int n = 0;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(EVP_MD_size(md(a))));
    if (!ctx || EVP_DigestInit_ex(ctx.get(), md(a), nullptr) != 1 || EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), out.data(), &n) != 1)
        return fail<std::vector<std::uint8_t>>(1603, "digest operation failed.");
    out.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto digest(HashAlgorithm a, std::string_view s) -> Result<std::vector<std::uint8_t>>
{
    return digest(a, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(s.data()), s.size()));
}
auto digestHex(HashAlgorithm a, std::span<const std::uint8_t> d) -> Result<std::string>
{
    auto r = digest(a, d);
    return r.ok ? Result<std::string>::Success(hex(r.value)) : Result<std::string>::Failure(r.error, r.message);
}
auto digestHex(HashAlgorithm a, std::string_view s) -> Result<std::string>
{
    return digestHex(a, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(s.data()), s.size()));
}
auto hmac(HashAlgorithm a, std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> Result<std::vector<std::uint8_t>>
{
    if (!md(a) || key.size() > static_cast<std::size_t>(INT_MAX) || data.size() > static_cast<std::size_t>(INT_MAX))
        return fail<std::vector<std::uint8_t>>(1600, "HMAC parameters invalid.");
    unsigned int n = 0;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(EVP_MD_size(md(a))));
    std::uint8_t dummy  = 0;
    const auto *keyPtr  = key.empty() ? &dummy : key.data();
    const auto *dataPtr = data.empty() ? &dummy : data.data();
    if (!HMAC(md(a), keyPtr, static_cast<int>(key.size()), dataPtr, data.size(), out.data(), &n))
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

auto randomBytes(std::size_t size) -> Result<std::vector<std::uint8_t>>
{
    if (size > static_cast<std::size_t>(INT_MAX))
        return fail<std::vector<std::uint8_t>>(1720, "random size invalid.");
    std::vector<std::uint8_t> out(size);
    if (size && RAND_bytes(out.data(), static_cast<int>(size)) != 1)
        return fail<std::vector<std::uint8_t>>(1720, "random generation failed.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto constantTimeEqual(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) noexcept -> bool
{
    std::uint8_t d = static_cast<std::uint8_t>(a.size() ^ b.size());
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        d |= a[i] ^ b[i];
    return d == 0 && a.size() == b.size();
}
auto sha256KeyId(std::span<const std::uint8_t> k) -> Result<std::string>
{
    return digestHex(HashAlgorithm::Sha256, k);
}

auto derivePbkdf2Sha256(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, std::uint32_t iterations,
                        std::size_t length) -> Result<std::vector<std::uint8_t>>
{
    if (salt.empty() || !iterations || !length || length > 1024 * 1024 || length > INT_MAX)
        return fail<std::vector<std::uint8_t>>(1730, "PBKDF2 parameters invalid.");
    std::vector<std::uint8_t> out(length);
    if (PKCS5_PBKDF2_HMAC(reinterpret_cast<const char *>(password.data()), static_cast<int>(password.size()), salt.data(),
                          static_cast<int>(salt.size()), iterations, EVP_sha256(), static_cast<int>(length), out.data()) != 1)
        return fail<std::vector<std::uint8_t>>(1731, "PBKDF2 failed.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}

auto aes256GcmEncrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> plain, std::span<const std::uint8_t> aad)
    -> Result<AesGcmCiphertext>
{
    auto n = randomBytes(12);
    if (!n.ok)
        return Result<AesGcmCiphertext>::Failure(n.error, n.message);
    return aes256GcmEncryptWithNonce(key, n.value, plain, aad);
}
auto aes256GcmEncryptWithNonce(std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
                               std::span<const std::uint8_t> plain, std::span<const std::uint8_t> aad) -> Result<AesGcmCiphertext>
{
    if (key.size() != 32 || nonce.size() != 12)
        return fail<AesGcmCiphertext>(1720, "AES-GCM parameters invalid.");
    CipherCtx c(EVP_CIPHER_CTX_new());
    AesGcmCiphertext o{{nonce.begin(), nonce.end()}, std::vector<std::uint8_t>(plain.size()), std::vector<std::uint8_t>(16)};
    int n = 0, total = 0;
    std::uint8_t dummy = 0;
    auto *out          = o.ciphertext.empty() ? &dummy : o.ciphertext.data();
    if (!c || EVP_EncryptInit_ex(c.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_EncryptInit_ex(c.get(), nullptr, nullptr, key.data(), nonce.data()) != 1 ||
        EVP_EncryptUpdate(c.get(), nullptr, &n, aad.data(), static_cast<int>(aad.size())) != 1 ||
        EVP_EncryptUpdate(c.get(), out, &n, plain.data(), static_cast<int>(plain.size())) != 1 ||
        EVP_EncryptFinal_ex(c.get(), out + n, &total) != 1 ||
        EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_GET_TAG, 16, o.tag.data()) != 1)
        return fail<AesGcmCiphertext>(1721, "AES-GCM encryption failed.");
    o.ciphertext.resize(n + total);
    return Result<AesGcmCiphertext>::Success(std::move(o));
}
auto aes256GcmDecrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
                      std::span<const std::uint8_t> cipher, std::span<const std::uint8_t> tag, std::span<const std::uint8_t> aad)
    -> Result<std::vector<std::uint8_t>>
{
    if (key.size() != 32 || nonce.size() != 12 || tag.size() != 16)
        return fail<std::vector<std::uint8_t>>(1720, "AES-GCM parameters invalid.");
    CipherCtx c(EVP_CIPHER_CTX_new());
    std::vector<std::uint8_t> out(cipher.size());
    int n = 0, total = 0;
    std::uint8_t dummy = 0;
    auto *plain        = out.empty() ? &dummy : out.data();
    if (!c || EVP_DecryptInit_ex(c.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
        EVP_DecryptInit_ex(c.get(), nullptr, nullptr, key.data(), nonce.data()) != 1 ||
        EVP_DecryptUpdate(c.get(), nullptr, &n, aad.data(), static_cast<int>(aad.size())) != 1 ||
        EVP_DecryptUpdate(c.get(), plain, &n, cipher.data(), static_cast<int>(cipher.size())) != 1 ||
        EVP_CIPHER_CTX_ctrl(c.get(), EVP_CTRL_GCM_SET_TAG, 16, const_cast<std::uint8_t *>(tag.data())) != 1 ||
        EVP_DecryptFinal_ex(c.get(), plain + n, &total) != 1)
        return fail<std::vector<std::uint8_t>>(1722, "AES-GCM authentication failed.");
    out.resize(n + total);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}

auto generateRsa3072KeyPair() -> Result<RsaKeyPair>
{
    PkeyCtx c(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    EVP_PKEY *raw = nullptr;
    if (!c || EVP_PKEY_keygen_init(c.get()) != 1 || EVP_PKEY_CTX_set_rsa_keygen_bits(c.get(), 3072) != 1 ||
        EVP_PKEY_keygen(c.get(), &raw) != 1)
        return fail<RsaKeyPair>(1711, "RSA key generation failed.");
    Pkey p(raw);
    auto pub  = serializePublic(p.get());
    auto priv = serializePrivate(p.get());
    if (!pub.ok || !priv.ok)
        return fail<RsaKeyPair>(1711, "RSA key serialization failed.");
    return Result<RsaKeyPair>::Success({std::move(pub.value), std::move(priv.value)});
}
auto rsaPublicKeyToDer(std::span<const std::uint8_t> k) -> Result<std::vector<std::uint8_t>>
{
    auto p = derPublic(k);
    if (!p)
        return fail<std::vector<std::uint8_t>>(1710, "invalid RSA public key.");
    return serializePublic(p.get());
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
auto rsaCrypt(bool enc, std::span<const std::uint8_t> key, std::span<const std::uint8_t> in) -> Result<std::vector<std::uint8_t>>
{
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
    std::vector<std::uint8_t> out(n);
    if ((enc ? EVP_PKEY_encrypt(c.get(), out.data(), &n, in.data(), in.size()) :
               EVP_PKEY_decrypt(c.get(), out.data(), &n, in.data(), in.size())) != 1)
        return fail<std::vector<std::uint8_t>>(1712, "RSA OAEP operation failed.");
    out.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
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
    PkeyCtx c(EVP_PKEY_CTX_new(p.get(), nullptr));
    if (!c || EVP_PKEY_sign_init(c.get()) != 1 || EVP_PKEY_CTX_set_rsa_padding(c.get(), RSA_PKCS1_PSS_PADDING) != 1 ||
        EVP_PKEY_CTX_set_signature_md(c.get(), EVP_sha256()) != 1 || EVP_PKEY_CTX_set_rsa_pss_saltlen(c.get(), 32) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA PSS initialization failed.");
    auto h   = digest(HashAlgorithm::Sha256, d);
    size_t n = 0;
    if (!h.ok || EVP_PKEY_sign(c.get(), nullptr, &n, h.value.data(), h.value.size()) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA signing failed.");
    std::vector<std::uint8_t> out(n);
    if (EVP_PKEY_sign(c.get(), out.data(), &n, h.value.data(), h.value.size()) != 1)
        return fail<std::vector<std::uint8_t>>(1711, "RSA signing failed.");
    out.resize(n);
    return Result<std::vector<std::uint8_t>>::Success(std::move(out));
}
auto rsaPssSha256Verify(std::span<const std::uint8_t> k, std::span<const std::uint8_t> d, std::span<const std::uint8_t> sig)
    -> Result<bool>
{
    Pkey p = derPublic(k);
    if (!p)
        return fail<bool>(1710, "invalid RSA public key.");
    PkeyCtx c(EVP_PKEY_CTX_new(p.get(), nullptr));
    auto h = digest(HashAlgorithm::Sha256, d);
    if (!c || !h.ok || EVP_PKEY_verify_init(c.get()) != 1 || EVP_PKEY_CTX_set_rsa_padding(c.get(), RSA_PKCS1_PSS_PADDING) != 1 ||
        EVP_PKEY_CTX_set_signature_md(c.get(), EVP_sha256()) != 1 || EVP_PKEY_CTX_set_rsa_pss_saltlen(c.get(), 32) != 1)
        return fail<bool>(1711, "RSA verification failed.");
    return Result<bool>::Success(EVP_PKEY_verify(c.get(), sig.data(), sig.size(), h.value.data(), h.value.size()) == 1);
}

// Fernet remains protocol-compatible while using OpenSSL AES-CBC/HMAC.
auto encryptFernet(std::string_view plain, std::string_view keyText) -> Result<std::string>
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
    return Result<std::string>::Success(encodeBase64(t, Base64Alphabet::UrlSafe));
}
auto decryptFernet(std::string_view tokenText, std::string_view keyText) -> Result<std::vector<std::uint8_t>>
{
    auto k = decodeBase64(keyText, Base64Alphabet::UrlSafe);
    auto t = decodeBase64(StringHelper::Strip(std::string(tokenText)), Base64Alphabet::UrlSafe);
    if (!k.ok || !t.ok || k.value.size() != 32 || t.value.size() < 57 || t.value[0] != 0x80)
        return fail<std::vector<std::uint8_t>>(1601, "invalid Fernet token.");
    auto n = t.value.size() - 32;
    auto m = hmac(HashAlgorithm::Sha256, std::span<const std::uint8_t>(k.value).first(16),
                  std::span<const std::uint8_t>(t.value).first(n));
    if (!m.ok || !constantTimeEqual(m.value, std::span<const std::uint8_t>(t.value).subspan(n)))
        return fail<std::vector<std::uint8_t>>(1601, "Fernet authentication failed.");
    return crypt(false, std::span<const std::uint8_t>(k.value).subspan(16), std::span<const std::uint8_t>(t.value).subspan(9, 16),
                 std::span<const std::uint8_t>(t.value).subspan(25, n - 25));
}
} // namespace SZX::CRYPTO
