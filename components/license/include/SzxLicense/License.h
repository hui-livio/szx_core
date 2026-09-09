#pragma once

#include "Result.hpp"
#include "SzxLicense/LicenseExport.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::LICENSE
{
enum class LicenseError : int
{
    InvalidArgument  = 1800,
    Malformed        = 1801,
    Unsupported      = 1802,
    SignatureInvalid = 1803,
    Expired          = 1804,
    NotYetValid      = 1805,
    WrongProduct     = 1806,
    MachineMismatch  = 1807,
    IoError          = 1808,
};

struct MachineFactor
{
    std::string type;
    std::string hash;
};

struct RequestOptions
{
    std::string productId;
    std::string appVersion;
    std::vector<std::string> requestedFeatures;
    std::map<std::string, std::string> customFields;
};

struct IssueOptions
{
    std::string licenseId;
    std::string issuer;
    std::vector<std::string> features;
    std::map<std::string, std::string> customFields;
    std::int64_t notBeforeUtc{};
    std::optional<std::int64_t> expiresAtUtc;
};

struct LicenseInfo
{
    std::string licenseId;
    std::string productId;
    std::string appVersion;
    std::string issuer;
    std::int64_t issuedAtUtc{};
    std::int64_t notBeforeUtc{};
    std::optional<std::int64_t> expiresAtUtc;
    std::vector<std::string> features;
    std::vector<MachineFactor> machineFactors;
    std::map<std::string, std::string> customFields;
};

/** Collects normalized, product-scoped machine factor hashes. Raw hardware identifiers are never returned. */
[[nodiscard]] SZX_LICENSE_API auto collectMachineFactors(std::string_view productId)
    -> SZX::COMMON::Result<std::vector<MachineFactor>>;
[[nodiscard]] SZX_LICENSE_API auto createRequest(const RequestOptions &options) -> SZX::COMMON::Result<std::string>;
/** Generates a request and atomically writes the UTF-8 JSON to the requested path. */
[[nodiscard]] SZX_LICENSE_API auto createRequestFile(const RequestOptions &options, std::string_view outputPath,
                                                     std::span<const std::uint8_t> issuerPublicKey) -> SZX::COMMON::Result<void>;
[[nodiscard]] SZX_LICENSE_API auto verify(std::span<const std::uint8_t> envelope, std::span<const std::uint8_t> issuerPublicKey,
                                          std::string_view expectedProduct = {}) -> SZX::COMMON::Result<LicenseInfo>;
/** Reads a bounded license envelope from disk and verifies it. */
[[nodiscard]] SZX_LICENSE_API auto verifyFile(std::string_view licensePath, std::span<const std::uint8_t> issuerPublicKey,
                                              std::string_view expectedProduct = {}) -> SZX::COMMON::Result<LicenseInfo>;
[[nodiscard]] SZX_LICENSE_API auto hasFeature(const LicenseInfo &license, std::string_view feature) noexcept -> bool;
} // namespace SZX::LICENSE
