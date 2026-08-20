/**
 * @file Hash.cc
 * @brief Hash and HMAC helper implementation.
 */

#include "SzxCrypto/Hash.h"

#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

#if defined(_WIN32)
    #include <Windows.h>
    #include <bcrypt.h>
#endif

using namespace SZX::COMMON;

namespace SZX::CRYPTO
{
namespace
{
constexpr int kHashError = 1603;

#if defined(_WIN32)
struct BCryptAlgorithmDeleter
{
    void operator()(BCRYPT_ALG_HANDLE handle) const noexcept
    {
        if (handle != nullptr)
            BCryptCloseAlgorithmProvider(handle, 0);
    }
};

struct BCryptHashDeleter
{
    void operator()(BCRYPT_HASH_HANDLE handle) const noexcept
    {
        if (handle != nullptr)
            BCryptDestroyHash(handle);
    }
};

using BCryptAlgorithm = std::unique_ptr<void, BCryptAlgorithmDeleter>;
using BCryptHash      = std::unique_ptr<void, BCryptHashDeleter>;

[[nodiscard]] auto ntSuccess(NTSTATUS status) -> bool
{
    return status >= 0;
}

[[nodiscard]] auto algorithmName(HashAlgorithm algorithm) -> LPCWSTR
{
    switch (algorithm)
    {
    case HashAlgorithm::Sha1:
        return BCRYPT_SHA1_ALGORITHM;
    case HashAlgorithm::Sha256:
        return BCRYPT_SHA256_ALGORITHM;
    case HashAlgorithm::Sha512:
        return BCRYPT_SHA512_ALGORITHM;
    }
    return BCRYPT_SHA256_ALGORITHM;
}
#endif

[[nodiscard]] auto bytesToHex(std::span<const std::uint8_t> bytes) -> std::string
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : bytes)
        output << std::setw(2) << static_cast<int>(byte);
    return output.str();
}

} // namespace

auto digest(HashAlgorithm algorithm, std::span<const std::uint8_t> data) -> Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE rawAlgorithm = nullptr;
    if (!ntSuccess(BCryptOpenAlgorithmProvider(&rawAlgorithm, algorithmName(algorithm), nullptr, 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to open hash provider.");
    BCryptAlgorithm algorithmHandle(rawAlgorithm);

    DWORD hashLength = 0;
    DWORD resultSize = 0;
    if (!ntSuccess(BCryptGetProperty(rawAlgorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength),
                                     &resultSize, 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to read hash length.");
    }

    BCRYPT_HASH_HANDLE rawHash = nullptr;
    if (!ntSuccess(BCryptCreateHash(rawAlgorithm, &rawHash, nullptr, 0, nullptr, 0, 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to create hash.");
    BCryptHash hash(rawHash);

    if (!ntSuccess(BCryptHashData(rawHash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to hash data.");

    std::vector<std::uint8_t> output(hashLength);
    if (!ntSuccess(BCryptFinishHash(rawHash, output.data(), static_cast<ULONG>(output.size()), 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to finish hash.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(output));
#else
    (void)algorithm;
    (void)data;
    return Result<std::vector<std::uint8_t>>::Failure(kHashError, "hash helpers are only implemented on Windows.");
#endif
}

auto digest(HashAlgorithm algorithm, std::string_view data) -> Result<std::vector<std::uint8_t>>
{
    return digest(algorithm, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
}

auto digestHex(HashAlgorithm algorithm, std::span<const std::uint8_t> data) -> Result<std::string>
{
    auto bytes = digest(algorithm, data);
    if (!bytes.ok)
        return Result<std::string>::Failure(bytes.error, bytes.message);
    return Result<std::string>::Success(bytesToHex(bytes.value));
}

auto digestHex(HashAlgorithm algorithm, std::string_view data) -> Result<std::string>
{
    return digestHex(algorithm, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
}

auto hmac(HashAlgorithm algorithm, std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> Result<std::vector<std::uint8_t>>
{
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE rawAlgorithm = nullptr;
    if (!ntSuccess(BCryptOpenAlgorithmProvider(&rawAlgorithm, algorithmName(algorithm), nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to open HMAC provider.");
    BCryptAlgorithm algorithmHandle(rawAlgorithm);

    DWORD hashLength = 0;
    DWORD resultSize = 0;
    if (!ntSuccess(BCryptGetProperty(rawAlgorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength),
                                     &resultSize, 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to read HMAC length.");
    }

    BCRYPT_HASH_HANDLE rawHash = nullptr;
    if (!ntSuccess(BCryptCreateHash(rawAlgorithm, &rawHash, nullptr, 0, const_cast<PUCHAR>(key.data()),
                                    static_cast<ULONG>(key.size()), 0)))
    {
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to create HMAC.");
    }
    BCryptHash hash(rawHash);

    if (!ntSuccess(BCryptHashData(rawHash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to hash HMAC data.");

    std::vector<std::uint8_t> output(hashLength);
    if (!ntSuccess(BCryptFinishHash(rawHash, output.data(), static_cast<ULONG>(output.size()), 0)))
        return Result<std::vector<std::uint8_t>>::Failure(kHashError, "failed to finish HMAC.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(output));
#else
    (void)algorithm;
    (void)key;
    (void)data;
    return Result<std::vector<std::uint8_t>>::Failure(kHashError, "HMAC helpers are only implemented on Windows.");
#endif
}

auto hmac(HashAlgorithm algorithm, std::string_view key, std::string_view data) -> Result<std::vector<std::uint8_t>>
{
    return hmac(algorithm, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(key.data()), key.size()),
                std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
}

auto hmacHex(HashAlgorithm algorithm, std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> Result<std::string>
{
    auto bytes = hmac(algorithm, key, data);
    if (!bytes.ok)
        return Result<std::string>::Failure(bytes.error, bytes.message);
    return Result<std::string>::Success(bytesToHex(bytes.value));
}

auto hmacHex(HashAlgorithm algorithm, std::string_view key, std::string_view data) -> Result<std::string>
{
    return hmacHex(algorithm, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(key.data()), key.size()),
                   std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
}

} // namespace SZX::CRYPTO
