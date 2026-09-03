#include <openssl/evp.h>
#include "OpenSslInternal.hpp"
#include "SzxCrypto/Aead.h"
#include "SzxCrypto/Random.h"
using namespace SZX::COMMON;
namespace SZX::CRYPTO
{
using namespace internal;
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
    if (key.size() != 32 || nonce.size() != 12 || !fitsIntPair(plain.size(), aad.size()))
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
    if (key.size() != 32 || nonce.size() != 12 || tag.size() != 16 || !fitsIntPair(cipher.size(), aad.size()))
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
} // namespace SZX::CRYPTO
