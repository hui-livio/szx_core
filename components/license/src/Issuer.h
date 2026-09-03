#pragma once

#include "SzxLicense/License.h"

namespace SZX::LICENSE
{
// Internal issuer-only API. This header is not installed with the customer SDK.
[[nodiscard]] SZX_LICENSE_API auto issue(const std::string &requestJson, const IssueOptions &options,
                                          std::span<const std::uint8_t> issuerPrivateKey,
                                          std::span<const std::uint8_t> issuerPublicKey)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>;
} // namespace SZX::LICENSE
