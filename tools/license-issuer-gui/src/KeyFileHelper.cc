#include "KeyFileHelper.h"

#include "FileHelper.hpp"
#include "SzxCrypto/Rsa.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace SzxLicenseIssuerGui
{
auto readBytes(std::string_view path, std::uintmax_t maxBytes) -> std::optional<std::vector<std::uint8_t>>
{
    return SZX::COMMON::FileHelper::ReadBytes(std::filesystem::path(path), maxBytes);
}

namespace
{
auto readKey(std::string_view path, bool privateKey) -> std::optional<std::vector<std::uint8_t>>
{
    auto bytes = readBytes(path);
    if (!bytes || bytes->empty())
        return std::nullopt;
    const std::string_view content(reinterpret_cast<const char *>(bytes->data()), bytes->size());
    if (content.find("-----BEGIN") == std::string_view::npos)
        return bytes;
    auto converted = privateKey ? SZX::CRYPTO::rsaPrivateKeyFromPem(*bytes) : SZX::CRYPTO::rsaPublicKeyFromPem(*bytes);
    if (!converted.ok)
        return std::nullopt;
    return std::move(converted.value);
}
} // namespace

auto readPublicKey(std::string_view path) -> std::optional<std::vector<std::uint8_t>>
{
    return readKey(path, false);
}

auto readPrivateKey(std::string_view path) -> std::optional<std::vector<std::uint8_t>>
{
    return readKey(path, true);
}

auto writeBytes(std::string_view path, const std::vector<std::uint8_t> &bytes, bool overwrite) -> bool
{
    const auto target = std::filesystem::path(path);
    if (target.empty() || (!overwrite && SZX::COMMON::FileHelper::Exists(target)))
        return false;
    const auto temporary = target.string() + ".tmp";
    if (!SZX::COMMON::FileHelper::WriteBytes(temporary, bytes, true))
        return false;
    std::error_code error;
    if (overwrite)
        std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}
} // namespace SzxLicenseIssuerGui
