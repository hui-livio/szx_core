/**
 * @file Fernet.h
 * @brief 兼容 Python cryptography.fernet 的加解密接口。
 *
 * 提供与 Python cryptography.fernet 包兼容的 AES-128-CBC 与 HMAC-SHA256
 * token 加解密。
 */

#pragma once

#include "SzxCrypto/CryptoExport.h"

#include "Result.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace SZX::CRYPTO
{

/**
 * @brief 将 Fernet token 解密为明文字节。
 *
 * 密钥必须是 URL-safe Base64 编码的 32 字节 Fernet 密钥。解密前会先验证
 * token 完整性，篡改 token 不会返回明文。
 *
 * @param[in] encryptedToken URL-safe Base64 编码的 Fernet token。
 * @param[in] urlSafeBase64Key URL-safe Base64 编码的 32 字节 Fernet 密钥。
 * @return 成功时返回明文；密钥/token 格式错误、认证失败或 provider 错误时返回失败。
 *
 * @note 本接口不校验 token 的 TTL 和时间戳新鲜度。
 */
[[nodiscard]] SZX_CRYPTO_API auto decryptFernet(std::string_view encryptedToken, std::string_view urlSafeBase64Key)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/**
 * @brief 将明文字节加密为 Fernet token。
 *
 * 密钥必须是 URL-safe Base64 编码的 32 字节 Fernet 密钥。生成的 token 包含
 * 当前系统时间戳和随机初始化向量。
 *
 * @param[in] plainData 待加密的明文字节。
 * @param[in] urlSafeBase64Key URL-safe Base64 编码的 32 字节 Fernet 密钥。
 * @return 成功时返回 URL-safe Base64 token；密钥格式错误或 provider 错误时返回失败。
 */
[[nodiscard]] SZX_CRYPTO_API auto encryptFernet(std::string_view plainData, std::string_view urlSafeBase64Key)
    -> SZX::COMMON::Result<std::string>;

} // namespace SZX::CRYPTO
