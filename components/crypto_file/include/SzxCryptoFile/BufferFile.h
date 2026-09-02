#pragma once
#include "Result.hpp"
#include "SzxCryptoFile/CryptoFileExport.h"
#include "SzxCrypto/Aead.h"
#include <cstdint>
#include <span>
#include <vector>
namespace SZX::CRYPTO_FILE {
[[nodiscard]] auto encryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> plaintext) -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
[[nodiscard]] auto decryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> container) -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
}
