#pragma once

#include "Serializer.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace SZX::LICENSE::internal
{
struct MachineFactorJson
{
    std::optional<std::string> type;
    std::optional<std::string> hash;
};

struct RequestDocument
{
    std::optional<std::int64_t> schema_version;
    std::optional<std::string> request_id;
    std::optional<std::string> product_id;
    std::optional<std::string> app_version;
    std::optional<std::int64_t> created_at;
    std::optional<std::string> nonce;
    std::optional<std::vector<MachineFactorJson>> machine_factors;
    std::optional<std::vector<std::string>> requested_features;
    std::optional<std::map<std::string, std::string>> custom_fields;
};

struct LicensePayload
{
    std::optional<std::int64_t> schema_version;
    std::optional<std::string> license_id;
    std::optional<std::string> product_id;
    std::optional<std::string> app_version;
    std::optional<std::string> issuer;
    std::optional<std::int64_t> issued_at;
    std::optional<std::int64_t> not_before;
    std::optional<std::int64_t> expires_at;
    std::optional<std::vector<std::string>> features;
    std::optional<std::vector<MachineFactorJson>> machine_factors;
    std::optional<std::map<std::string, std::string>> custom_fields;
};

SZX_REFL(MachineFactorJson, type, hash)
SZX_REFL(RequestDocument, schema_version, request_id, product_id, app_version, created_at, nonce, machine_factors,
         requested_features, custom_fields)
SZX_REFL(LicensePayload, schema_version, license_id, product_id, app_version, issuer, issued_at, not_before, expires_at,
         features, machine_factors, custom_fields)
} // namespace SZX::LICENSE::internal
