/**
 * @file Aead.h
 * @brief AES-256-GCM 认证加密接口。
 */
#pragma once

#include "Result.hpp"
#include "SzxCrypto/CryptoExport.h"

#include <cstdint>
#include <span>
#include <vector>

namespace SZX::CRYPTO
{

enum class AeadError : int
{
    InvalidArgument = 1720,
    ProviderFailure = 1721,
    AuthenticationFailed = 1722,
    Unsupported = 1723,
};

struct AesGcmCiphertext
{
    std::vector<std::uint8_t> nonce;
    std::vector<std::uint8_t> ciphertext;
    std::vector<std::uint8_t> tag;
};

/** 使用系统随机 96 位 nonce 进行 AES-256-GCM 加密。 */
[[nodiscard]] SZX_CRYPTO_API auto aes256GcmEncrypt(std::span<const std::uint8_t> key,
                                                    std::span<const std::uint8_t> plaintext,
                                                    std::span<const std::uint8_t> aad = {})
    -> SZX::COMMON::Result<AesGcmCiphertext>;

/** 使用调用方提供的 nonce 进行 AES-256-GCM 加密，主要用于协议固定向量。 */
[[nodiscard]] SZX_CRYPTO_API auto aes256GcmEncryptWithNonce(std::span<const std::uint8_t> key,
                                                            std::span<const std::uint8_t> nonce,
                                                            std::span<const std::uint8_t> plaintext,
                                                            std::span<const std::uint8_t> aad = {})
    -> SZX::COMMON::Result<AesGcmCiphertext>;

/** 验证 tag 后解密 AES-256-GCM 数据。 */
[[nodiscard]] SZX_CRYPTO_API auto aes256GcmDecrypt(std::span<const std::uint8_t> key,
                                                    std::span<const std::uint8_t> nonce,
                                                    std::span<const std::uint8_t> ciphertext,
                                                    std::span<const std::uint8_t> tag,
                                                    std::span<const std::uint8_t> aad = {})
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

} // namespace SZX::CRYPTO
