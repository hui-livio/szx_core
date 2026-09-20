#pragma once

#include "SzxCryptoService/CryptoService.h"

namespace SZX::ADMIN::AUTH
{
/** Stored credential version: Argon2id of the 64 lowercase hex characters sent by the client. */
inline constexpr std::string_view kPasswordHashPrefix = "sha256-v1:";

[[nodiscard]] auto validClientPasswordHash(std::string_view hash) noexcept -> bool;
[[nodiscard]] auto storeClientPasswordHash(SZX::CRYPTO_SERVICE::ICryptoService &crypto, std::string_view hash)
    -> SZX::COMMON::Result<std::string>;
[[nodiscard]] auto verifyClientPasswordHash(SZX::CRYPTO_SERVICE::ICryptoService &crypto, std::string_view hash,
                                            std::string_view stored) -> SZX::COMMON::Result<bool>;
} // namespace SZX::ADMIN::AUTH
