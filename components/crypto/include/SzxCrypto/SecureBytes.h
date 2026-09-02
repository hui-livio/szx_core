/**
 * @file SecureBytes.h
 * @brief 用于保存密钥等敏感字节的可移动安全容器。
 */
#pragma once

#include "SzxCrypto/CryptoExport.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace SZX::CRYPTO
{

/** 不可复制、析构时清零的敏感字节容器。 */
class SZX_CRYPTO_API SecureBytes final
{
public:
    /** 创建空容器。 */
    SecureBytes() = default;
    /** 接管字节并在析构时清零。 */
    explicit SecureBytes(std::vector<std::uint8_t> bytes);
    /** 禁止复制，避免敏感数据产生额外副本。 */
    SecureBytes(const SecureBytes &)                     = delete;
    auto operator=(const SecureBytes &) -> SecureBytes & = delete;
    /** 移动容器所有权。 */
    SecureBytes(SecureBytes &&other) noexcept;
    auto operator=(SecureBytes &&other) noexcept -> SecureBytes &;
    ~SecureBytes() noexcept;

    /** 返回当前字节数。 */
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto empty() const noexcept -> bool;
    /** 返回只读数据指针；空容器返回 nullptr。 */
    [[nodiscard]] auto data() const noexcept -> const std::uint8_t *;
    [[nodiscard]] auto span() const noexcept -> std::span<const std::uint8_t>;
    /** 移出字节；调用后容器为空。 */
    [[nodiscard]] auto take() && noexcept -> std::vector<std::uint8_t>;
    /** 清零并清空敏感数据。 */
    void clear() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SZX::CRYPTO
