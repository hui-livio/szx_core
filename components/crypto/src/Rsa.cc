/** @file Rsa.cc */
#include "SzxCrypto/Rsa.h"

#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"

#include <memory>
#include <algorithm>
#include <cstring>
#include <utility>

#if defined(_WIN32)
    #include <Windows.h>
    #include <bcrypt.h>
#endif

namespace SZX::CRYPTO
{
namespace
{
template <typename T>
auto failure(RsaError error, const char *message) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(error), message);
}

#if defined(_WIN32)
struct AlgorithmCloser
{
    void operator()(void *handle) const noexcept
    {
        if (handle != nullptr)
            BCryptCloseAlgorithmProvider(reinterpret_cast<BCRYPT_ALG_HANDLE>(handle), 0);
    }
};
struct KeyCloser
{
    void operator()(void *handle) const noexcept
    {
        if (handle != nullptr)
            BCryptDestroyKey(reinterpret_cast<BCRYPT_KEY_HANDLE>(handle));
    }
};
using Algorithm = std::unique_ptr<void, AlgorithmCloser>;
using Key = std::unique_ptr<void, KeyCloser>;

auto ntSuccess(NTSTATUS status) -> bool { return status >= 0; }

auto openRsa() -> Algorithm
{
    BCRYPT_ALG_HANDLE raw = nullptr;
    if (!ntSuccess(BCryptOpenAlgorithmProvider(&raw, BCRYPT_RSA_ALGORITHM, nullptr, 0)))
        return {};
    return Algorithm(raw);
}

auto importKey(BCRYPT_ALG_HANDLE algorithm, std::span<const std::uint8_t> blob, LPCWSTR blobType) -> Key
{
    BCRYPT_KEY_HANDLE raw = nullptr;
    if (!ntSuccess(BCryptImportKeyPair(algorithm, nullptr, blobType, &raw, const_cast<PUCHAR>(blob.data()),
                                       static_cast<ULONG>(blob.size()), 0)))
        return {};
    return Key(raw);
}

auto exportKey(BCRYPT_KEY_HANDLE key, LPCWSTR blobType) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    ULONG size = 0;
    if (!ntSuccess(BCryptExportKey(key, nullptr, blobType, nullptr, 0, &size, 0)))
        return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to size RSA key export.");
    std::vector<std::uint8_t> result(size);
    if (!ntSuccess(BCryptExportKey(key, nullptr, blobType, result.data(), size, &size, 0)))
        return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to export RSA key.");
    result.resize(size);
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(result));
}
#endif

auto derLength(std::size_t size) -> std::vector<std::uint8_t>
{
    if (size < 128) return {static_cast<std::uint8_t>(size)};
    std::vector<std::uint8_t> bytes;
    while (size) { bytes.push_back(static_cast<std::uint8_t>(size)); size >>= 8; }
    std::reverse(bytes.begin(), bytes.end());
    bytes.insert(bytes.begin(), static_cast<std::uint8_t>(0x80 | bytes.size()));
    return bytes;
}
auto derWrap(std::uint8_t tag, const std::vector<std::uint8_t> &value) -> std::vector<std::uint8_t>
{
    std::vector<std::uint8_t> out{tag}; auto len = derLength(value.size()); out.insert(out.end(), len.begin(), len.end());
    out.insert(out.end(), value.begin(), value.end()); return out;
}
auto derInteger(std::span<const std::uint8_t> value) -> std::vector<std::uint8_t>
{
    while (value.size() > 1 && value.front() == 0) value = value.subspan(1);
    std::vector<std::uint8_t> bytes(value.begin(), value.end());
    if (bytes.empty() || (bytes.front() & 0x80)) bytes.insert(bytes.begin(), 0);
    return derWrap(0x02, bytes);
}
} // namespace

auto generateRsa3072KeyPair() -> SZX::COMMON::Result<RsaKeyPair>
{
#if defined(_WIN32)
    auto algorithm = openRsa();
    if (!algorithm)
        return failure<RsaKeyPair>(RsaError::ProviderFailure, "failed to open RSA provider.");
    BCRYPT_KEY_HANDLE rawKey = nullptr;
    if (!ntSuccess(BCryptGenerateKeyPair(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm.get()), &rawKey, 3072, 0)))
        return failure<RsaKeyPair>(RsaError::ProviderFailure, "failed to generate RSA key pair.");
    Key key(rawKey);
    if (!ntSuccess(BCryptFinalizeKeyPair(rawKey, 0)))
        return failure<RsaKeyPair>(RsaError::ProviderFailure, "failed to finalize RSA key pair.");
    auto publicKey = exportKey(rawKey, BCRYPT_RSAPUBLIC_BLOB);
    auto privateKey = exportKey(rawKey, BCRYPT_RSAFULLPRIVATE_BLOB);
    if (!publicKey.ok || !privateKey.ok)
        return failure<RsaKeyPair>(RsaError::ProviderFailure, "failed to export RSA key pair.");
    return SZX::COMMON::Result<RsaKeyPair>::Success({std::move(publicKey.value), std::move(privateKey.value)});
#else
    return failure<RsaKeyPair>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

auto rsaPublicKeyId(std::span<const std::uint8_t> publicKey) -> SZX::COMMON::Result<std::string>
{
    if (publicKey.empty())
        return failure<std::string>(RsaError::InvalidArgument, "RSA 公钥 blob 不能为空。");
    return sha256KeyId(publicKey);
}

auto rsaPublicKeyToDer(std::span<const std::uint8_t> publicKey) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    if (publicKey.size() < sizeof(BCRYPT_RSAKEY_BLOB)) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "RSA 公钥 blob 长度无效。");
    const auto *blob = reinterpret_cast<const BCRYPT_RSAKEY_BLOB *>(publicKey.data());
    if (blob->Magic != BCRYPT_RSAPUBLIC_MAGIC || blob->cbPublicExp == 0 || blob->cbModulus == 0 ||
        sizeof(BCRYPT_RSAKEY_BLOB) + blob->cbPublicExp + blob->cbModulus > publicKey.size())
        return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "RSA 公钥 blob 格式无效。");
    const auto *exponent = publicKey.data() + sizeof(BCRYPT_RSAKEY_BLOB);
    const auto *modulus = exponent + blob->cbPublicExp;
    auto rsa = derWrap(0x30, [&] { std::vector<std::uint8_t> v; auto n = derInteger({modulus, blob->cbModulus}); auto e = derInteger({exponent, blob->cbPublicExp}); v.insert(v.end(), n.begin(), n.end()); v.insert(v.end(), e.begin(), e.end()); return v; }());
    std::vector<std::uint8_t> bitString{0}; bitString.insert(bitString.end(), rsa.begin(), rsa.end());
    auto algorithm = derWrap(0x30, {0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00});
    std::vector<std::uint8_t> body = algorithm; auto bits = derWrap(0x03, bitString); body.insert(body.end(), bits.begin(), bits.end());
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(derWrap(0x30, body));
#else
    (void)publicKey; return failure<std::vector<std::uint8_t>>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

auto rsaPublicKeyFromDer(std::span<const std::uint8_t> der) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    auto read = [](std::span<const std::uint8_t> input, std::size_t &pos, std::uint8_t tag, std::span<const std::uint8_t> &value) -> bool {
        if (pos >= input.size() || input[pos++] != tag || pos >= input.size()) return false; const auto first = input[pos++]; std::size_t len = first;
        if (first & 0x80) { const auto count = first & 0x7f; if (count == 0 || count > sizeof(std::size_t) || pos + count > input.size()) return false; len = 0; for (std::size_t i=0;i<count;++i) len = (len << 8) | input[pos++]; }
        if (pos + len > input.size()) return false; value = input.subspan(pos, len); pos += len; return true;
    };
    std::size_t p = 0; std::span<const std::uint8_t> outer, alg, bits, rsa, n, e;
    if (!read(der, p, 0x30, outer) || p != der.size()) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "DER 外层结构无效。");
    p = 0; if (!read(outer, p, 0x30, alg) || !read(outer, p, 0x03, bits) || p != outer.size() || bits.size() < 1 || bits[0] != 0) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "DER SubjectPublicKeyInfo 无效。");
    const std::uint8_t oid[] = {0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01,0x05,0x00};
    if (!std::equal(alg.begin(), alg.end(), std::begin(oid), std::end(oid))) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "DER RSA 算法标识无效。");
    p = 1; if (!read(bits, p, 0x30, rsa) || p != bits.size()) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "DER RSA 公钥无效。");
    p = 0; if (!read(rsa, p, 0x02, n) || !read(rsa, p, 0x02, e) || p != rsa.size() || n.empty() || e.empty() || n.front() == 0 && n.size() > 1 && !(n[1] & 0x80)) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "DER RSA 参数无效。");
    while (n.size() > 1 && n.front() == 0) n = n.subspan(1); while (e.size() > 1 && e.front() == 0) e = e.subspan(1);
    BCRYPT_RSAKEY_BLOB header{BCRYPT_RSAPUBLIC_MAGIC, static_cast<ULONG>(n.size() * 8), static_cast<ULONG>(e.size()), static_cast<ULONG>(n.size()), 0, 0};
    std::vector<std::uint8_t> out(sizeof(header) + e.size() + n.size()); std::memcpy(out.data(), &header, sizeof(header)); std::copy(e.begin(), e.end(), out.begin() + sizeof(header)); std::copy(n.begin(), n.end(), out.begin() + sizeof(header) + e.size());
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out));
#else
    (void)der; return failure<std::vector<std::uint8_t>>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

auto rsaOaepSha256Encrypt(std::span<const std::uint8_t> publicKey, std::span<const std::uint8_t> plaintext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    auto algorithm = openRsa();
    if (!algorithm) return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to open RSA provider.");
    auto key = importKey(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm.get()), publicKey, BCRYPT_RSAPUBLIC_BLOB);
    if (!key) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "invalid RSA public key blob.");
    BCRYPT_OAEP_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, nullptr, 0}; ULONG size = 0;
    auto status = BCryptEncrypt(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), const_cast<PUCHAR>(plaintext.data()), static_cast<ULONG>(plaintext.size()), &padding, nullptr, 0, nullptr, 0, &size, BCRYPT_PAD_OAEP);
    if (!ntSuccess(status)) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "RSA-OAEP plaintext is too large.");
    std::vector<std::uint8_t> result(size);
    if (!ntSuccess(BCryptEncrypt(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), const_cast<PUCHAR>(plaintext.data()), static_cast<ULONG>(plaintext.size()), &padding, nullptr, 0, result.data(), size, &size, BCRYPT_PAD_OAEP)))
        return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "RSA-OAEP encryption failed.");
    result.resize(size); return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(result));
#else
    (void)publicKey; (void)plaintext; return failure<std::vector<std::uint8_t>>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

auto rsaOaepSha256Decrypt(std::span<const std::uint8_t> privateKey, std::span<const std::uint8_t> ciphertext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    auto algorithm = openRsa();
    if (!algorithm) return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to open RSA provider.");
    auto key = importKey(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm.get()), privateKey, BCRYPT_RSAFULLPRIVATE_BLOB);
    if (!key) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "invalid RSA private key blob.");
    BCRYPT_OAEP_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, nullptr, 0}; ULONG size = 0;
    auto status = BCryptDecrypt(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), const_cast<PUCHAR>(ciphertext.data()), static_cast<ULONG>(ciphertext.size()), &padding, nullptr, 0, nullptr, 0, &size, BCRYPT_PAD_OAEP);
    if (!ntSuccess(status)) return failure<std::vector<std::uint8_t>>(RsaError::AuthenticationFailed, "RSA-OAEP ciphertext is invalid.");
    std::vector<std::uint8_t> result(size);
    if (!ntSuccess(BCryptDecrypt(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), const_cast<PUCHAR>(ciphertext.data()), static_cast<ULONG>(ciphertext.size()), &padding, nullptr, 0, result.data(), size, &size, BCRYPT_PAD_OAEP)))
        return failure<std::vector<std::uint8_t>>(RsaError::AuthenticationFailed, "RSA-OAEP decryption failed.");
    result.resize(size); return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(result));
#else
    (void)privateKey; (void)ciphertext; return failure<std::vector<std::uint8_t>>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

auto rsaPssSha256Sign(std::span<const std::uint8_t> privateKey, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    auto algorithm = openRsa();
    if (!algorithm) return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to open RSA provider.");
    auto key = importKey(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm.get()), privateKey, BCRYPT_RSAFULLPRIVATE_BLOB);
    if (!key) return failure<std::vector<std::uint8_t>>(RsaError::InvalidArgument, "invalid RSA private key blob.");
    auto hash = digest(HashAlgorithm::Sha256, data);
    if (!hash.ok) return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to hash RSA signing input.");
    BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 32}; ULONG size = 0;
    if (!ntSuccess(BCryptSignHash(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), &padding, hash.value.data(), static_cast<ULONG>(hash.value.size()), nullptr, 0, &size, BCRYPT_PAD_PSS)))
        return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "failed to size RSA signature.");
    std::vector<std::uint8_t> result(size);
    if (!ntSuccess(BCryptSignHash(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), &padding, hash.value.data(), static_cast<ULONG>(hash.value.size()), result.data(), size, &size, BCRYPT_PAD_PSS)))
        return failure<std::vector<std::uint8_t>>(RsaError::ProviderFailure, "RSA signing failed.");
    result.resize(size); return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(result));
#else
    (void)privateKey; (void)data; return failure<std::vector<std::uint8_t>>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

auto rsaPssSha256Verify(std::span<const std::uint8_t> publicKey, std::span<const std::uint8_t> data, std::span<const std::uint8_t> signature)
    -> SZX::COMMON::Result<bool>
{
#if defined(_WIN32)
    auto algorithm = openRsa();
    if (!algorithm) return failure<bool>(RsaError::ProviderFailure, "failed to open RSA provider.");
    auto key = importKey(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm.get()), publicKey, BCRYPT_RSAPUBLIC_BLOB);
    if (!key) return failure<bool>(RsaError::InvalidArgument, "invalid RSA public key blob.");
    auto hash = digest(HashAlgorithm::Sha256, data);
    if (!hash.ok) return failure<bool>(RsaError::ProviderFailure, "failed to hash RSA verification input.");
    BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 32};
    const auto status = BCryptVerifySignature(reinterpret_cast<BCRYPT_KEY_HANDLE>(key.get()), &padding, hash.value.data(), static_cast<ULONG>(hash.value.size()), const_cast<PUCHAR>(signature.data()), static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS);
    if (!ntSuccess(status))
        return SZX::COMMON::Result<bool>::Success(false);
    return SZX::COMMON::Result<bool>::Success(true);
#else
    (void)publicKey; (void)data; (void)signature; return failure<bool>(RsaError::Unsupported, "RSA CNG is only available on Windows.");
#endif
}

} // namespace SZX::CRYPTO
