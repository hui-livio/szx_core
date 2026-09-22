#include "KeyFileHelper.h"

#include "FileHelper.hpp"
#include "SzxCrypto/Rsa.h"

#include <filesystem>

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
    auto converted = privateKey ? SZX::CRYPTO::rsaPrivateKeyFromEncoded(*bytes) : SZX::CRYPTO::rsaPublicKeyFromEncoded(*bytes);
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
    const auto backup    = target.string() + ".bak";
    std::error_code error;
    std::filesystem::remove(temporary, error);
    std::filesystem::remove(backup, error);
    if (!SZX::COMMON::FileHelper::WriteBytes(temporary, bytes, true))
        return false;

    const bool hadTarget = std::filesystem::exists(target, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    if (hadTarget)
    {
        if (!overwrite)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        error.clear();
        std::filesystem::rename(target, backup, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
    if (error)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        if (hadTarget)
        {
            error.clear();
            std::filesystem::rename(backup, target, error);
        }
        return false;
    }
    if (hadTarget)
        std::filesystem::remove(backup, error);
    return true;
}
} // namespace SzxLicenseIssuerGui
