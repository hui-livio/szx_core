#include "SzxLicense/License.h"
#include "Issuer.h"

#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
#include "SzxCrypto/Rsa.h"
#include "SzxCryptoService/CryptoService.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace SZX::LICENSE
{
using SZX::COMMON::Result;
using Json = nlohmann::json;
namespace
{
constexpr std::uint16_t kVersion               = 1;
constexpr std::uint16_t kAlgorithmRsaPssSha256 = 1;
constexpr std::size_t kMaxPayload              = 1024 * 1024;
constexpr std::size_t kMaxRequest              = 1024 * 1024;
constexpr std::size_t kMaxMachineFactors       = 8;
constexpr std::size_t kMaxMachineFactorType    = 64;
constexpr std::size_t kRequiredMachineMatches  = 2;

template <typename T>
auto fail(LicenseError error, const char *message) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(error), message);
}
auto nowUtc() -> std::int64_t
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
void put16(std::vector<std::uint8_t> &out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}
void put32(std::vector<std::uint8_t> &out, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
}
auto read16(const std::vector<std::uint8_t> &in, std::size_t &p) -> std::optional<std::uint16_t>
{
    if (p + 2 > in.size())
        return std::nullopt;
    auto v  = static_cast<std::uint16_t>(in[p] | (in[p + 1] << 8));
    p      += 2;
    return v;
}
auto read32(const std::vector<std::uint8_t> &in, std::size_t &p) -> std::optional<std::uint32_t>
{
    if (p + 4 > in.size())
        return std::nullopt;
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<std::uint32_t>(in[p++]) << (i * 8);
    return v;
}
auto jsonArray(const std::vector<std::string> &values) -> Json
{
    return Json(values);
}
auto fieldsJson(const std::map<std::string, std::string> &fields) -> Json
{
    Json out = Json::object();
    for (const auto &[key, value] : fields)
        out[key] = value;
    return out;
}
auto hexBytes(std::string_view text) -> std::optional<std::vector<std::uint8_t>>
{
    if (text.size() != 64)
        return std::nullopt;
    std::vector<std::uint8_t> out;
    out.reserve(32);
    auto value = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < text.size(); i += 2)
    {
        const auto hi = value(text[i]), lo = value(text[i + 1]);
        if (hi < 0 || lo < 0)
            return std::nullopt;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}
auto trim(std::string value) -> std::string
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    value            = value.substr(first, last - first + 1);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}
auto rawMachineFactors() -> std::vector<std::pair<std::string, std::string>>
{
    std::vector<std::pair<std::string, std::string>> factors;
#ifdef _WIN32
    const auto appendSmbiosUuid = [&] {
        constexpr DWORD kRawSmbios = 'RSMB';
        const auto size = GetSystemFirmwareTable(kRawSmbios, 0, nullptr, 0);
        if (size == 0)
            return;
        std::vector<std::uint8_t> table(size);
        if (GetSystemFirmwareTable(kRawSmbios, 0, table.data(), size) != size || size < 8)
            return;
        std::uint32_t tableLength = 0;
        std::memcpy(&tableLength, table.data() + 4, sizeof(tableLength));
        const auto end = std::min<std::size_t>(table.size(), 8ULL + tableLength);
        std::size_t offset = 8;
        while (offset + 4 <= end)
        {
            const auto type = table[offset], length = table[offset + 1];
            if (length < 4 || offset + length > end)
                break;
            if (type == 1 && length >= 0x19)
            {
                const auto *uuid = table.data() + offset + 8;
                const bool empty = std::all_of(uuid, uuid + 16, [](std::uint8_t byte) { return byte == 0 || byte == 0xff; });
                if (!empty)
                {
                    static constexpr char digits[] = "0123456789abcdef";
                    std::string value;
                    value.reserve(32);
                    for (int i = 0; i < 16; ++i)
                    {
                        value.push_back(digits[uuid[i] >> 4]);
                        value.push_back(digits[uuid[i] & 0x0f]);
                    }
                    factors.emplace_back("smbios_uuid", std::move(value));
                }
                break;
            }
            std::size_t next = offset + length;
            while (next + 1 < end && (table[next] != 0 || table[next + 1] != 0))
                ++next;
            offset = next + 2;
        }
    };
    appendSmbiosUuid();
    HKEY key{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
    {
        wchar_t value[256]{};
        DWORD size = sizeof(value), type = 0;
        if (RegQueryValueExW(key, L"MachineGuid", nullptr, &type, reinterpret_cast<BYTE *>(value), &size) == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ))
        {
            char utf8[768]{};
            const auto chars = std::min<DWORD>(size / sizeof(wchar_t), static_cast<DWORD>(std::size(value) - 1));
            value[chars] = L'\0';
            const int length = WideCharToMultiByte(CP_UTF8, 0, value, static_cast<int>(chars), utf8, sizeof(utf8) - 1, nullptr, nullptr);
            if (length > 1)
                factors.emplace_back("machine_guid", utf8);
        }
        RegCloseKey(key);
    }
    wchar_t windowsDir[MAX_PATH]{};
    wchar_t root[MAX_PATH]{};
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH) != 0 && GetVolumePathNameW(windowsDir, root, MAX_PATH) != 0)
    {
        DWORD serial    = 0;
        if (GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0) != 0)
            factors.emplace_back("system_volume", std::to_string(serial));
    }
#else
    std::ifstream machineId("/etc/machine-id");
    std::string value;
    if (machineId && std::getline(machineId, value))
        factors.emplace_back("machine_id", value);
#endif
    return factors;
}
auto machineFactorsJson(const std::vector<MachineFactor> &factors) -> Json
{
    Json out = Json::array();
    for (const auto &factor : factors)
        out.push_back(Json{{"type", factor.type}, {"hash", factor.hash}});
    return out;
}
auto parseMachineFactors(const Json &value) -> std::optional<std::vector<MachineFactor>>
{
    if (!value.is_array())
        return std::nullopt;
    std::vector<MachineFactor> out;
    for (const auto &entry : value)
    {
        if (!entry.is_object() || !entry.contains("type") || !entry.contains("hash") || !entry.at("type").is_string() ||
            !entry.at("hash").is_string())
            return std::nullopt;
        auto type = entry.at("type").get<std::string>();
        auto hash = entry.at("hash").get<std::string>();
        if (type.empty() || type.size() > kMaxMachineFactorType || hash.size() != 64 || !hexBytes(hash))
            return std::nullopt;
        if (std::any_of(out.begin(), out.end(), [&](const auto &factor) { return factor.type == type; }))
            return std::nullopt;
        out.push_back({std::move(type), std::move(hash)});
        if (out.size() > kMaxMachineFactors)
            return std::nullopt;
    }
    return out;
}
auto validMachineFactors(const std::vector<MachineFactor> &factors) -> bool
{
    if (factors.empty() || factors.size() > kMaxMachineFactors)
        return false;
    for (const auto &factor : factors)
        if (factor.type.empty() || factor.type.size() > kMaxMachineFactorType || factor.hash.size() != 64 || !hexBytes(factor.hash))
            return false;
    for (std::size_t i = 0; i < factors.size(); ++i)
        for (std::size_t j = i + 1; j < factors.size(); ++j)
            if (factors[i].type == factors[j].type)
                return false;
    return true;
}
} // namespace

auto collectMachineFactors(std::string_view productId) -> Result<std::vector<MachineFactor>>
{
    try
    {
        if (productId.empty())
            return fail<std::vector<MachineFactor>>(LicenseError::InvalidArgument, "product is required.");
        std::vector<MachineFactor> out;
        for (const auto &[type, raw] : rawMachineFactors())
        {
            const auto normalizedType = trim(type), normalizedRaw = trim(raw);
            if (normalizedType.empty() || normalizedRaw.empty())
                continue;
            auto hashed = SZX::CRYPTO::digestHex(SZX::CRYPTO::HashAlgorithm::Sha256,
                                                 "SZX-LICENSE-MACHINE-V1|" + std::string(productId) + "|" + normalizedType + "|" + normalizedRaw);
            if (!hashed.ok)
                return Result<std::vector<MachineFactor>>::Failure(hashed.error, hashed.message);
            out.push_back({normalizedType, std::move(hashed.value)});
        }
        if (out.empty())
            return fail<std::vector<MachineFactor>>(LicenseError::Unsupported, "machine identifiers are unavailable.");
        return Result<std::vector<MachineFactor>>::Success(std::move(out));
    }
    catch (...)
    {
        return fail<std::vector<MachineFactor>>(LicenseError::Malformed, "machine factor collection failed.");
    }
}

auto createRequest(const RequestOptions &options) -> Result<std::string>
{
    try
    {
        if (options.productId.empty() || options.appVersion.empty())
            return fail<std::string>(LicenseError::InvalidArgument, "product and version are required.");
        auto nonce = SZX::CRYPTO::randomBytes(16);
        if (!nonce.ok)
            return Result<std::string>::Failure(nonce.error, nonce.message);
        auto encoded = SZX::CRYPTO::encodeBase64(nonce.value, SZX::CRYPTO::Base64Alphabet::UrlSafe, false);
        if (!encoded.ok)
            return Result<std::string>::Failure(encoded.error, encoded.message);
        auto machineFactors = collectMachineFactors(options.productId);
        if (!machineFactors.ok)
            return Result<std::string>::Failure(machineFactors.error, machineFactors.message);
        if (!validMachineFactors(machineFactors.value))
            return fail<std::string>(LicenseError::InvalidArgument, "invalid machine factors.");
        Json j{{"app_version", options.appVersion},
               {"created_at", nowUtc()},
               {"custom_fields", fieldsJson(options.customFields)},
               {"nonce", encoded.value},
               {"machine_factors", machineFactorsJson(machineFactors.value)},
               {"product_id", options.productId},
               {"requested_features", jsonArray(options.requestedFeatures)},
               {"request_id", encoded.value},
               {"schema_version", 1}};
        auto text = j.dump();
        if (text.size() > kMaxRequest)
            return fail<std::string>(LicenseError::InvalidArgument, "request payload too large.");
        return Result<std::string>::Success(std::move(text));
    } catch (...)
    {
        return fail<std::string>(LicenseError::Malformed, "request generation failed.");
    }
}

auto issue(const std::string &requestJson, const IssueOptions &options, std::span<const std::uint8_t> issuerPrivateKey,
           std::span<const std::uint8_t> issuerPublicKey) -> Result<std::vector<std::uint8_t>>
{
    try
    {
        if (requestJson.empty() || requestJson.size() > kMaxRequest)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "request payload size invalid.");
        auto req           = Json::parse(requestJson);
        if (req.value("schema_version", 0) != 1)
            return fail<std::vector<std::uint8_t>>(LicenseError::Unsupported, "unsupported request format.");
        const auto product = req.at("product_id").get<std::string>();
        const auto version = req.at("app_version").get<std::string>();
        const auto requestMachineFactors = req.contains("machine_factors") ? parseMachineFactors(req.at("machine_factors"))
                                                                              : std::optional<std::vector<MachineFactor>>{};
        if (!requestMachineFactors || requestMachineFactors->empty())
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "machine factors are required.");
        if (product.empty() || version.empty() || options.licenseId.empty() || options.issuer.empty() || issuerPrivateKey.empty() ||
            issuerPublicKey.empty())
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "invalid issue parameters.");
        auto keyId = SZX::CRYPTO::rsaPublicKeyId(issuerPublicKey);
        if (!keyId.ok)
            return Result<std::vector<std::uint8_t>>::Failure(keyId.error, keyId.message);
        auto keyIdBytes = hexBytes(keyId.value);
        if (!keyIdBytes)
            return fail<std::vector<std::uint8_t>>(LicenseError::Malformed, "invalid issuer key id.");
        const auto issued    = nowUtc();
        const auto notBefore = options.notBeforeUtc ? options.notBeforeUtc : issued;
        if (options.expiresAtUtc && *options.expiresAtUtc <= notBefore)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "license expiry must be after start.");
        const auto features =
            options.features.empty() ? req.value("requested_features", std::vector<std::string>{}) : options.features;
        Json payload{{"app_version", version},
                     {"custom_fields", fieldsJson(options.customFields)},
                     {"expires_at", options.expiresAtUtc ? Json(*options.expiresAtUtc) : Json(nullptr)},
                     {"features", jsonArray(features)},
                     {"issued_at", issued},
                     {"issuer", options.issuer},
                     {"license_id", options.licenseId},
                     {"machine_factors", machineFactorsJson(*requestMachineFactors)},
                     {"not_before", notBefore},
                     {"product_id", product},
                     {"schema_version", 1}};
        const auto text = payload.dump();
        if (text.size() > kMaxPayload)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "license payload too large.");
        std::vector<std::uint8_t> signedBytes{'S', 'Z', 'X', 'L'};
        put16(signedBytes, kVersion);
        put16(signedBytes, kAlgorithmRsaPssSha256);
        // Store the raw 32-byte SHA-256 key ID in the signed envelope.
        signedBytes.insert(signedBytes.end(), keyIdBytes->begin(), keyIdBytes->end());
        put32(signedBytes, static_cast<std::uint32_t>(text.size()));
        signedBytes.insert(signedBytes.end(), text.begin(), text.end());
        SZX::CRYPTO_SERVICE::CryptoService crypto;
        auto signature = crypto.sign(issuerPrivateKey, signedBytes);
        if (!signature.ok)
            return Result<std::vector<std::uint8_t>>::Failure(signature.error, signature.message);
        auto selfCheck = crypto.verify(issuerPublicKey, signedBytes, signature.value);
        if (!selfCheck.ok || !selfCheck.value)
            return fail<std::vector<std::uint8_t>>(LicenseError::SignatureInvalid, "issuer key pair mismatch.");
        std::vector<std::uint8_t> envelope = signedBytes;
        put32(envelope, static_cast<std::uint32_t>(signature.value.size()));
        envelope.insert(envelope.end(), signature.value.begin(), signature.value.end());
        return Result<std::vector<std::uint8_t>>::Success(std::move(envelope));
    } catch (...)
    {
        return fail<std::vector<std::uint8_t>>(LicenseError::Malformed, "license issue failed.");
    }
}

auto verify(std::span<const std::uint8_t> bytes, std::span<const std::uint8_t> issuerPublicKey, std::string_view expectedProduct)
    -> Result<LicenseInfo>
{
    try
    {
        std::vector<std::uint8_t> in(bytes.begin(), bytes.end());
        if (in.size() < 12 || !std::equal(in.begin(), in.begin() + 4, "SZXL"))
            return fail<LicenseInfo>(LicenseError::Malformed, "invalid license envelope.");
        std::size_t p  = 4;
        auto version   = read16(in, p);
        auto algorithm = read16(in, p);
        if (!version || !algorithm || *version != kVersion || *algorithm != kAlgorithmRsaPssSha256)
            return fail<LicenseInfo>(LicenseError::Unsupported, "unsupported license format.");
        if (p + 32 > in.size())
            return fail<LicenseInfo>(LicenseError::Malformed, "truncated license key id.");
        auto keyId      = SZX::CRYPTO::rsaPublicKeyId(issuerPublicKey);
        auto keyIdBytes = keyId.ok ? hexBytes(keyId.value) : std::nullopt;
        if (!keyIdBytes || !std::equal(keyIdBytes->begin(), keyIdBytes->end(), in.begin() + p, in.begin() + p + 32))
            return fail<LicenseInfo>(LicenseError::SignatureInvalid, "unknown issuer key.");
        p                += 32;
        auto payloadSize  = read32(in, p);
        if (!payloadSize || *payloadSize > kMaxPayload || p + *payloadSize + 4 > in.size())
            return fail<LicenseInfo>(LicenseError::Malformed, "invalid license payload.");
        const auto signedEnd = p + *payloadSize;
        std::vector<std::uint8_t> signedBytes(in.begin(), in.begin() + signedEnd);
        std::string text(in.begin() + p, in.begin() + signedEnd);
        p            = signedEnd;
        auto sigSize = read32(in, p);
        if (!sigSize || *sigSize == 0 || p + *sigSize != in.size())
            return fail<LicenseInfo>(LicenseError::Malformed, "invalid license signature.");
        SZX::CRYPTO_SERVICE::CryptoService crypto;
        auto valid = crypto.verify(issuerPublicKey, signedBytes, std::span<const std::uint8_t>(in).subspan(p, *sigSize));
        if (!valid.ok || !valid.value)
            return fail<LicenseInfo>(LicenseError::SignatureInvalid, "license signature invalid.");
        auto payload = Json::parse(text);
        if (payload.value("schema_version", 0) != 1 || !payload.contains("expires_at") ||
            !payload.contains("issuer") || !payload.contains("license_id") || !payload.contains("product_id") ||
            !payload.contains("app_version") || !payload.contains("issued_at") || !payload.contains("not_before") ||
            !payload.contains("features") || !payload.contains("custom_fields"))
            return fail<LicenseInfo>(LicenseError::Malformed, "license payload fields invalid.");
        auto machineFactors = payload.contains("machine_factors") ? parseMachineFactors(payload.at("machine_factors"))
                                                                     : std::optional<std::vector<MachineFactor>>(std::vector<MachineFactor>{});
        if (!machineFactors)
            return fail<LicenseInfo>(LicenseError::Malformed, "license machine factors invalid.");
        LicenseInfo out{payload.at("license_id").get<std::string>(),
                        payload.at("product_id").get<std::string>(),
                        payload.at("app_version").get<std::string>(),
                        payload.value("issuer", ""),
                        payload.at("issued_at").get<std::int64_t>(),
                        payload.at("not_before").get<std::int64_t>(),
                        std::nullopt,
                        payload.value("features", std::vector<std::string>{}),
                        *machineFactors,
                        payload.value("custom_fields", std::map<std::string, std::string>{})};
        if (!payload.at("expires_at").is_null())
            out.expiresAtUtc = payload.at("expires_at").get<std::int64_t>();
        if (out.licenseId.empty() || out.productId.empty() || out.appVersion.empty() || out.issuer.empty() ||
            (out.expiresAtUtc && *out.expiresAtUtc <= out.notBeforeUtc))
            return fail<LicenseInfo>(LicenseError::Malformed, "license payload values invalid.");
        if (!expectedProduct.empty() && out.productId != expectedProduct)
            return fail<LicenseInfo>(LicenseError::WrongProduct, "license product mismatch.");
        if (!out.machineFactors.empty())
        {
            auto local = collectMachineFactors(out.productId);
            if (!local.ok)
                return Result<LicenseInfo>::Failure(local.error, local.message);
            std::size_t matches = 0;
            std::vector<std::pair<std::string, std::string>> matched;
            for (const auto &expected : out.machineFactors)
                if (std::any_of(local.value.begin(), local.value.end(), [&](const auto &actual) {
                        return actual.type == expected.type && actual.hash == expected.hash;
                    }) &&
                    std::find(matched.begin(), matched.end(), std::pair{expected.type, expected.hash}) == matched.end())
                {
                    ++matches;
                    matched.emplace_back(expected.type, expected.hash);
                }
            const auto required = std::min(kRequiredMachineMatches, out.machineFactors.size());
            if (matches < required)
                return fail<LicenseInfo>(LicenseError::MachineMismatch, "license machine mismatch.");
        }
        const auto now = nowUtc();
        if (now < out.notBeforeUtc)
            return fail<LicenseInfo>(LicenseError::NotYetValid, "license is not yet valid.");
        if (out.expiresAtUtc && now >= *out.expiresAtUtc)
            return fail<LicenseInfo>(LicenseError::Expired, "license expired.");
        return Result<LicenseInfo>::Success(std::move(out));
    } catch (...)
    {
        return fail<LicenseInfo>(LicenseError::Malformed, "license verification failed.");
    }
}

auto hasFeature(const LicenseInfo &license, std::string_view feature) noexcept -> bool
{
    return std::find(license.features.begin(), license.features.end(), feature) != license.features.end();
}
} // namespace SZX::LICENSE
