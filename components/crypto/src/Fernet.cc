/**
 * @file Fernet.cc
 * @brief Python cryptography.fernet compatible decrypt implementation.
 */

#include "SzxCrypto/Fernet.h"

#include "StringHelper.hpp"
#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Hash.h"
#include "TimeHelper.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
    #include <Windows.h>
    #include <bcrypt.h>
#endif

using namespace SZX::COMMON;

namespace SZX::CRYPTO
{
namespace
{
constexpr int kFernetError               = 1601;
constexpr std::size_t kFernetKeyBytes    = 32;
constexpr std::size_t kFernetIvBytes     = 16;
constexpr std::size_t kFernetMacBytes    = 32;
constexpr std::size_t kFernetHeaderBytes = 1 + 8 + kFernetIvBytes;

#if defined(_WIN32)
struct BCryptAlgorithmDeleter
{
    void operator()(BCRYPT_ALG_HANDLE handle) const noexcept
    {
        if (handle != nullptr)
            BCryptCloseAlgorithmProvider(handle, 0);
    }
};

struct BCryptKeyDeleter
{
    void operator()(BCRYPT_KEY_HANDLE handle) const noexcept
    {
        if (handle != nullptr)
            BCryptDestroyKey(handle);
    }
};

using BCryptAlgorithm = std::unique_ptr<void, BCryptAlgorithmDeleter>;
using BCryptKey       = std::unique_ptr<void, BCryptKeyDeleter>;

[[nodiscard]] auto ntSuccess(NTSTATUS status) -> bool
{
    return status >= 0;
}
#endif

[[nodiscard]] auto secureEquals(std::span<const std::uint8_t> left, std::span<const std::uint8_t> right) -> bool
{
    if (left.size() != right.size())
        return false;
    std::uint8_t diff = 0;
    for (std::size_t index = 0; index < left.size(); ++index)
        diff |= static_cast<std::uint8_t>(left[index] ^ right[index]);
    return diff == 0;
}

[[nodiscard]] auto aes128CbcDecrypt(std::span<const std::uint8_t> cipherText, std::span<const std::uint8_t> key,
                                    std::span<const std::uint8_t> iv) -> Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE rawAlgorithm = nullptr;
    if (!ntSuccess(BCryptOpenAlgorithmProvider(&rawAlgorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to open AES provider.");
    BCryptAlgorithm algorithm(rawAlgorithm);

    auto chainingMode = std::wstring(BCRYPT_CHAIN_MODE_CBC);
    if (!ntSuccess(BCryptSetProperty(rawAlgorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(chainingMode.data()),
                                     static_cast<ULONG>((chainingMode.size() + 1) * sizeof(wchar_t)), 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to set AES-CBC mode.");
    }

    BCRYPT_KEY_HANDLE rawKey = nullptr;
    if (!ntSuccess(BCryptGenerateSymmetricKey(rawAlgorithm, &rawKey, nullptr, 0, const_cast<PUCHAR>(key.data()),
                                              static_cast<ULONG>(key.size()), 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to create AES key.");
    }
    BCryptKey aesKey(rawKey);

    ULONG plainSize = 0;
    auto mutableIv  = std::vector<std::uint8_t>(iv.begin(), iv.end());
    if (!ntSuccess(BCryptDecrypt(rawKey, const_cast<PUCHAR>(cipherText.data()), static_cast<ULONG>(cipherText.size()), nullptr,
                                 mutableIv.data(), static_cast<ULONG>(mutableIv.size()), nullptr, 0, &plainSize,
                                 BCRYPT_BLOCK_PADDING)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to size AES decrypt output.");
    }

    std::vector<std::uint8_t> plain(plainSize);
    mutableIv.assign(iv.begin(), iv.end());
    if (!ntSuccess(BCryptDecrypt(rawKey, const_cast<PUCHAR>(cipherText.data()), static_cast<ULONG>(cipherText.size()), nullptr,
                                 mutableIv.data(), static_cast<ULONG>(mutableIv.size()), plain.data(),
                                 static_cast<ULONG>(plain.size()), &plainSize, BCRYPT_BLOCK_PADDING)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to decrypt Fernet token.");
    }
    plain.resize(plainSize);
    return Result<std::vector<std::uint8_t>>::Success(std::move(plain));
#else
    (void)cipherText;
    (void)key;
    (void)iv;
    return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "Fernet decrypt is only implemented on Windows.");
#endif
}

[[nodiscard]] auto aes128CbcEncrypt(std::span<const std::uint8_t> plainText, std::span<const std::uint8_t> key,
                                    std::span<const std::uint8_t> iv) -> Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE rawAlgorithm = nullptr;
    if (!ntSuccess(BCryptOpenAlgorithmProvider(&rawAlgorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to open AES provider.");
    BCryptAlgorithm algorithm(rawAlgorithm);

    auto chainingMode = std::wstring(BCRYPT_CHAIN_MODE_CBC);
    if (!ntSuccess(BCryptSetProperty(rawAlgorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(chainingMode.data()),
                                     static_cast<ULONG>((chainingMode.size() + 1) * sizeof(wchar_t)), 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to set AES-CBC mode.");
    }

    BCRYPT_KEY_HANDLE rawKey = nullptr;
    if (!ntSuccess(BCryptGenerateSymmetricKey(rawAlgorithm, &rawKey, nullptr, 0, const_cast<PUCHAR>(key.data()),
                                              static_cast<ULONG>(key.size()), 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to create AES key.");
    }
    BCryptKey aesKey(rawKey);

    ULONG cipherSize = 0;
    auto mutableIv   = std::vector<std::uint8_t>(iv.begin(), iv.end());
    if (!ntSuccess(BCryptEncrypt(rawKey, const_cast<PUCHAR>(plainText.data()), static_cast<ULONG>(plainText.size()), nullptr,
                                 mutableIv.data(), static_cast<ULONG>(mutableIv.size()), nullptr, 0, &cipherSize,
                                 BCRYPT_BLOCK_PADDING)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to size AES encrypt output.");
    }

    std::vector<std::uint8_t> cipher(cipherSize);
    mutableIv.assign(iv.begin(), iv.end());
    if (!ntSuccess(BCryptEncrypt(rawKey, const_cast<PUCHAR>(plainText.data()), static_cast<ULONG>(plainText.size()), nullptr,
                                 mutableIv.data(), static_cast<ULONG>(mutableIv.size()), cipher.data(),
                                 static_cast<ULONG>(cipher.size()), &cipherSize, BCRYPT_BLOCK_PADDING)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to encrypt Fernet token.");
    }
    cipher.resize(cipherSize);
    return Result<std::vector<std::uint8_t>>::Success(std::move(cipher));
#else
    (void)plainText;
    (void)key;
    (void)iv;
    return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "Fernet encrypt is only implemented on Windows.");
#endif
}

[[nodiscard]] auto randomBytes(std::size_t size) -> Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    std::vector<std::uint8_t> bytes(size);
    if (!ntSuccess(BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "failed to generate random bytes.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(bytes));
#else
    (void)size;
    return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "Fernet encrypt is only implemented on Windows.");
#endif
}

[[nodiscard]] auto decodeFernetKey(std::string_view urlSafeBase64Key) -> Result<std::vector<std::uint8_t>>
{
    auto key = decodeBase64(urlSafeBase64Key, Base64Alphabet::UrlSafe);
    if (!key.ok)
        return key;
    if (key.value.size() != kFernetKeyBytes)
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "Fernet key must decode to 32 bytes.");
    return key;
}

} // namespace

auto decryptFernet(std::string_view encryptedToken, std::string_view urlSafeBase64Key) -> Result<std::vector<std::uint8_t>>
{
    const auto tokenText = StringHelper::Strip(std::string(encryptedToken));
    if (tokenText.empty())
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "Fernet token is empty.");

    auto key = decodeFernetKey(urlSafeBase64Key);
    if (!key.ok)
        return key;

    auto token = decodeBase64(tokenText, Base64Alphabet::UrlSafe);
    if (!token.ok)
        return token;
    if (token.value.size() <= kFernetHeaderBytes + kFernetMacBytes || token.value.front() != 0x80)
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "invalid Fernet token.");

    const auto signedSize = token.value.size() - kFernetMacBytes;
    auto computedMac      = hmac(HashAlgorithm::Sha256, std::span<const std::uint8_t>(key.value).first(16),
                                 std::span<const std::uint8_t>(token.value).first(signedSize));
    if (!computedMac.ok)
        return Result<std::vector<std::uint8_t>>::Failure(computedMac.error, computedMac.message);
    if (!secureEquals(computedMac.value, std::span<const std::uint8_t>(token.value).subspan(signedSize, kFernetMacBytes)))
        return Result<std::vector<std::uint8_t>>::Failure(kFernetError, "Fernet token signature mismatch.");

    const auto encryptionKey = std::span<const std::uint8_t>(key.value).subspan(16, 16);
    const auto iv            = std::span<const std::uint8_t>(token.value).subspan(1 + 8, kFernetIvBytes);
    const auto cipherText =
        std::span<const std::uint8_t>(token.value).subspan(kFernetHeaderBytes, signedSize - kFernetHeaderBytes);
    return aes128CbcDecrypt(cipherText, encryptionKey, iv);
}

auto encryptFernet(std::string_view plainData, std::string_view urlSafeBase64Key) -> Result<std::string>
{
    auto key = decodeFernetKey(urlSafeBase64Key);
    if (!key.ok)
        return Result<std::string>::Failure(key.error, key.message);

    auto iv = randomBytes(kFernetIvBytes);
    if (!iv.ok)
        return Result<std::string>::Failure(iv.error, iv.message);

    const auto encryptionKey = std::span<const std::uint8_t>(key.value).subspan(16, 16);
    auto plainBytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(plainData.data()), plainData.size());
    auto cipherText = aes128CbcEncrypt(plainBytes, encryptionKey, iv.value);
    if (!cipherText.ok)
        return Result<std::string>::Failure(cipherText.error, cipherText.message);

    const auto timestamp = static_cast<std::uint64_t>(TimeHelper::NowTimestamp() / 1000);
    std::vector<std::uint8_t> token;
    token.reserve(kFernetHeaderBytes + cipherText.value.size() + kFernetMacBytes);
    token.push_back(0x80);
    for (int shift = 56; shift >= 0; shift -= 8)
        token.push_back(static_cast<std::uint8_t>((timestamp >> shift) & 0xFFU));
    token.insert(token.end(), iv.value.begin(), iv.value.end());
    token.insert(token.end(), cipherText.value.begin(), cipherText.value.end());

    auto mac = hmac(HashAlgorithm::Sha256, std::span<const std::uint8_t>(key.value).first(16), token);
    if (!mac.ok)
        return Result<std::string>::Failure(mac.error, mac.message);
    token.insert(token.end(), mac.value.begin(), mac.value.end());
    return Result<std::string>::Success(encodeBase64(token, Base64Alphabet::UrlSafe));
}

} // namespace SZX::CRYPTO
