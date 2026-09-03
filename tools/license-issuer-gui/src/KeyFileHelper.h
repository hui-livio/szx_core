#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace SzxLicenseIssuerGui
{
auto readPublicKey(std::string_view path) -> std::optional<std::vector<std::uint8_t>>;
auto readPrivateKey(std::string_view path) -> std::optional<std::vector<std::uint8_t>>;
auto readBytes(std::string_view path, std::uintmax_t maxBytes = 2U * 1024U * 1024U)
    -> std::optional<std::vector<std::uint8_t>>;
auto writeBytes(std::string_view path, const std::vector<std::uint8_t> &bytes, bool overwrite) -> bool;
} // namespace SzxLicenseIssuerGui
