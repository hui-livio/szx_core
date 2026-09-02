/** @file Rsa.cc */
#include "SzxCrypto/Rsa.h"

#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"

#include <memory>
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
