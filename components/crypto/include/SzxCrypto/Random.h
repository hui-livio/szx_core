/**
 * @file Random.h
 * @brief 密码学安全随机数和常量时间比较。
 */
#pragma once

#include "Result.hpp"
#include "SzxCrypto/CryptoExport.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace SZX::CRYPTO
{

/** 使用系统密码学随机源生成指定长度的随机字节。 */
[[nodiscard]] SZX_CRYPTO_API auto randomBytes(std::size_t size) -> SZX::COMMON::Result<std::vector<std::uint8_t>>;

/** 以常量时间比较两个字节序列，避免比较结果泄漏。 */
[[nodiscard]] SZX_CRYPTO_API auto constantTimeEqual(std::span<const std::uint8_t> left,
                                                    std::span<const std::uint8_t> right) noexcept -> bool;

/** 计算字节序列的 SHA-256 小写十六进制标识。 */
[[nodiscard]] SZX_CRYPTO_API auto sha256KeyId(std::span<const std::uint8_t> key) -> SZX::COMMON::Result<std::string>;

} // namespace SZX::CRYPTO
