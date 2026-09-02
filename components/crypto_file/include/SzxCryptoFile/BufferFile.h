#pragma once
/** @file BufferFile.h @brief `.szxe` 内存容器的加密、解密和校验接口。 */
#include "Result.hpp"
#include "SzxCryptoFile/CryptoFileExport.h"

#include <cstdint>
#include <span>
#include <vector>
namespace SZX::CRYPTO_SERVICE { class ICryptoService; }
namespace SZX::CRYPTO_FILE
{
/** `.szxe` 容器的已解析元数据。 */
struct ContainerInfo
{
    std::uint32_t version{};        ///< 容器格式版本。
    std::uint32_t ciphertextSize{}; ///< 密文块长度（字节）。
};
/** 使用指定加密服务创建单块 `.szxe` 容器。认证数据包含固定格式标识和版本。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto encryptBuffer(SZX::CRYPTO_SERVICE::ICryptoService &service, std::span<const std::uint8_t> key, std::span<const std::uint8_t> plaintext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用默认加密服务创建单块 `.szxe` 容器。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto encryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> plaintext)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用指定加密服务解析并认证解密 `.szxe` 容器；认证失败时不返回明文。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto decryptBuffer(SZX::CRYPTO_SERVICE::ICryptoService &service, std::span<const std::uint8_t> key, std::span<const std::uint8_t> container)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用默认加密服务解析并认证解密 `.szxe` 容器。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto decryptBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> container)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
/** 使用指定加密服务验证容器，不向调用方返回明文。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto verifyBuffer(SZX::CRYPTO_SERVICE::ICryptoService &service, std::span<const std::uint8_t> key, std::span<const std::uint8_t> container)
    -> SZX::COMMON::Result<void>;
/** 使用默认加密服务验证容器，不向调用方返回明文。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto verifyBuffer(std::span<const std::uint8_t> key, std::span<const std::uint8_t> container) -> SZX::COMMON::Result<void>;
/** 仅解析并校验容器头和长度，不执行认证。 */
[[nodiscard]] SZX_CRYPTO_FILE_API auto inspectBuffer(std::span<const std::uint8_t> container) -> SZX::COMMON::Result<ContainerInfo>;
} // namespace SZX::CRYPTO_FILE
