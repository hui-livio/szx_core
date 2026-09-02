/** @file CryptoService.h @brief 统一加密服务门面。 */
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include "Result.hpp"
#include "SzxCryptoService/CryptoServiceExport.h"
namespace SZX::CRYPTO_SERVICE
{
struct CryptoCiphertext
{
    std::vector<std::uint8_t> nonce;
    std::vector<std::uint8_t> ciphertext;
    std::vector<std::uint8_t> tag;
};

struct CryptoCapabilities
{
    bool aes256Gcm{};
    bool rsaOaepSha256{};
    bool rsaPssSha256{};
    bool pbkdf2Sha256{};
};

class ICryptoService
{
public:
    virtual ~ICryptoService() = default;

    virtual auto capabilities() const -> CryptoCapabilities = 0;

    virtual auto encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<CryptoCiphertext> = 0;

    virtual auto decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    virtual auto rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    virtual auto rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    virtual auto sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;

    virtual auto verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> = 0;

    virtual auto derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> = 0;
};

class SZX_CRYPTO_SERVICE_API CryptoService final : public ICryptoService
{
public:
    auto capabilities() const -> CryptoCapabilities override;

    auto encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<CryptoCiphertext> override;

    auto decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> override;

    auto derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;
};

class SZX_CRYPTO_SERVICE_API FakeCryptoService final : public ICryptoService
{
public:
    CryptoCapabilities supported{};
    std::size_t encryptCalls{};
    std::size_t decryptCalls{};
    auto capabilities() const -> CryptoCapabilities override
    {
        return supported;
    }

    auto encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<CryptoCiphertext> override;

    auto decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t> = {})
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;

    auto verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
        -> SZX::COMMON::Result<bool> override;

    auto derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
        -> SZX::COMMON::Result<std::vector<std::uint8_t>> override;
};
} // namespace SZX::CRYPTO_SERVICE
