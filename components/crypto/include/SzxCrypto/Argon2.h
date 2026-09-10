/**
 * @file Argon2.h
 * @brief Argon2id 密码哈希与校验接口。
 */
#pragma once

#include "Result.hpp"
#include "SzxCrypto/CryptoExport.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace SZX::CRYPTO
{

/** Argon2id 操作返回的稳定错误码。 */
enum class Argon2Error : int
{
    InvalidArgument      = 1740, ///< 参数超出安全范围。
    ProviderFailure      = 1741, ///< OpenSSL Argon2id provider 操作失败。
    AuthenticationFailed = 1742, ///< 密码校验失败（仅用于底层异常，不用于错误密码）。
    MalformedHash        = 1743, ///< PHC 编码的哈希字符串格式无效。
    Unsupported          = 1744, ///< 当前 OpenSSL provider 不支持 Argon2id。
};

/** Argon2id 的 PHC 参数；memoryCostKiB 为 1 KiB 内存块数量。 */
struct Argon2idParameters
{
    std::uint32_t memoryCostKiB{65536}; ///< 内存成本，单位 KiB。
    std::uint32_t iterations{3};        ///< 时间成本。
    std::uint32_t lanes{1};             ///< 并行 lane 数。
    std::size_t saltLength{16};         ///< 随机 salt 长度，字节。
    std::size_t hashLength{32};         ///< 输出哈希长度，字节。
};

/**
 * @brief 生成 PHC `$argon2id$...` 格式的密码哈希。
 *
 * 哈希字符串包含版本、参数、随机 salt 和摘要，可直接持久化到用户表。
 */
[[nodiscard]] SZX_CRYPTO_API auto hashArgon2id(std::span<const std::uint8_t> password, const Argon2idParameters &parameters = {})
    -> SZX::COMMON::Result<std::string>;

/**
 * @brief 校验 PHC 格式 Argon2id 哈希。
 *
 * 密码错误返回 `Success(false)`，格式错误或 provider 失败才返回失败 Result。
 */
[[nodiscard]] SZX_CRYPTO_API auto verifyArgon2id(std::span<const std::uint8_t> password, std::string_view encodedHash)
    -> SZX::COMMON::Result<bool>;

} // namespace SZX::CRYPTO
