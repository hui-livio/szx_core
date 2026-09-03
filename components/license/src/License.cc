#include "SzxLicense/License.h"
#include "Issuer.h"

#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Random.h"
#include "SzxCrypto/Rsa.h"
#include "SzxCryptoService/CryptoService.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

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
} // namespace

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
        Json j{{"app_version", options.appVersion},
               {"created_at", nowUtc()},
               {"custom_fields", fieldsJson(options.customFields)},
               {"nonce", encoded.value},
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
        LicenseInfo out{payload.at("license_id").get<std::string>(),
                        payload.at("product_id").get<std::string>(),
                        payload.at("app_version").get<std::string>(),
                        payload.value("issuer", ""),
                        payload.at("issued_at").get<std::int64_t>(),
                        payload.at("not_before").get<std::int64_t>(),
                        std::nullopt,
                        payload.value("features", std::vector<std::string>{}),
                        payload.value("custom_fields", std::map<std::string, std::string>{})};
        if (!payload.at("expires_at").is_null())
            out.expiresAtUtc = payload.at("expires_at").get<std::int64_t>();
        if (out.licenseId.empty() || out.productId.empty() || out.appVersion.empty() || out.issuer.empty() ||
            (out.expiresAtUtc && *out.expiresAtUtc <= out.notBeforeUtc))
            return fail<LicenseInfo>(LicenseError::Malformed, "license payload values invalid.");
        if (!expectedProduct.empty() && out.productId != expectedProduct)
            return fail<LicenseInfo>(LicenseError::WrongProduct, "license product mismatch.");
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
