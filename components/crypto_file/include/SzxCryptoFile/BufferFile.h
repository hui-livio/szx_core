#pragma once
#include "Result.hpp"
#include "SzxCryptoFile/CryptoFileExport.h"

#include <cstdint>
#include <span>
#include <vector>
namespace SZX::CRYPTO_FILE
{
struct ContainerInfo { std::uint32_t version{}; std::uint32_t ciphertextSize{}; };
[[nodiscard]] SZX_CRYPTO_FILE_API auto encryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> plaintext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
[[nodiscard]] SZX_CRYPTO_FILE_API auto decryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> container)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
[[nodiscard]] SZX_CRYPTO_FILE_API auto verifyBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> container) -> SZX::COMMON::Result<void>;
[[nodiscard]] SZX_CRYPTO_FILE_API auto inspectBuffer(std::span<const std::uint8_t> container) -> SZX::COMMON::Result<ContainerInfo>;
} // namespace SZX::CRYPTO_FILE
