#pragma once

#include "Result.hpp"
#include "SzxCrypto/Hash.h"

#include <algorithm>
#include <climits>
#include <iomanip>
#include <memory>
#include <openssl/evp.h>
#include <span>
#include <sstream>

namespace SZX::CRYPTO::internal
{
template <typename T, void (*Free)(T *)>
struct Deleter
{
    void operator()(T *p) const noexcept
    {
        if (p)
            Free(p);
    }
};
template <typename T, void (*Free)(T *)>
using Handle    = std::unique_ptr<T, Deleter<T, Free>>;
using MdCtx     = Handle<EVP_MD_CTX, EVP_MD_CTX_free>;
using CipherCtx = Handle<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free>;
using Pkey      = Handle<EVP_PKEY, EVP_PKEY_free>;
using PkeyCtx   = Handle<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;

template <typename T>
auto fail(int code, const char *message) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(code, message);
}
inline auto fitsInt(std::size_t value) noexcept -> bool
{
    return value <= static_cast<std::size_t>(INT_MAX);
}
inline auto fitsIntPair(std::size_t first, std::size_t second) noexcept -> bool
{
    return fitsInt(first) && fitsInt(second);
}
inline auto validAesCbcInput(std::span<const std::uint8_t> key, std::span<const std::uint8_t> iv, std::size_t inputSize) noexcept
    -> bool
{
    return key.size() == 16 && iv.size() == 16 && fitsInt(inputSize) && inputSize <= (static_cast<std::size_t>(-1) - 16);
}
inline auto validRsaInput(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data) noexcept -> bool
{
    return !key.empty() && fitsIntPair(key.size(), data.size());
}
inline auto constantTimeEqual(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) noexcept -> bool
{
    std::uint8_t diff = static_cast<std::uint8_t>(a.size() ^ b.size());
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    return diff == 0 && a.size() == b.size();
}
inline auto md(HashAlgorithm a) -> const EVP_MD *
{
    switch (a)
    {
    case HashAlgorithm::Sha1:
        return EVP_sha1();
    case HashAlgorithm::Sha256:
        return EVP_sha256();
    case HashAlgorithm::Sha512:
        return EVP_sha512();
    default:
        return nullptr;
    }
}
inline auto hex(std::span<const std::uint8_t> bytes) -> std::string
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto b : bytes)
        out << std::setw(2) << static_cast<unsigned>(b);
    return out.str();
}
} // namespace SZX::CRYPTO::internal
