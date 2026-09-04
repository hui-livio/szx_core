#include "SzxLicense/License.h"
#include "SzxLicense/Issuer.h"
#include "LicenseJson.hpp"

#include "FileHelper.hpp"
#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Hash.h"
#include "SzxCrypto/Random.h"
#include "SzxCrypto/Rsa.h"
#include "SzxCryptoService/CryptoService.h"
#include "TimeHelper.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace SZX::LICENSE
{
using SZX::COMMON::Result;
using namespace SZX::LICENSE::internal;
namespace
{
constexpr std::uint16_t kVersion               = 1;
constexpr std::uint16_t kAlgorithmRsaPssSha256 = 1;
constexpr std::size_t kMaxPayload              = 1024 * 1024;
constexpr std::size_t kMaxRequest              = 1024 * 1024;
constexpr std::size_t kMaxMachineFactors       = 8;
constexpr std::size_t kMaxMachineFactorType    = 64;
constexpr std::size_t kRequiredMachineMatches  = 2;
constexpr std::size_t kMaxFileBytes            = 2 * 1024 * 1024;
constexpr std::uint16_t kRequestVersion        = 1;
constexpr std::uint16_t kRequestAlgorithm      = 1; // RSA-OAEP-SHA256 + AES-256-GCM
constexpr std::size_t kRequestNonceSize        = 12;
constexpr std::size_t kRequestTagSize          = 16;

template <typename T>
auto fail(LicenseError error, const char *message) -> Result<T>
{
    return Result<T>::Failure(static_cast<int>(error), message);
}
auto nowUtc() -> std::int64_t
{
    return SZX::COMMON::TimeHelper::NowTimestamp() / 1000;
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
    value           = value.substr(first, last - first + 1);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}
auto rawMachineFactors() -> std::vector<std::pair<std::string, std::string>>
{
    std::vector<std::pair<std::string, std::string>> factors;
#ifdef _WIN32
    const auto appendSmbiosUuid = [&]
    {
        constexpr DWORD kRawSmbios = 'RSMB';
        const auto size            = GetSystemFirmwareTable(kRawSmbios, 0, nullptr, 0);
        if (size == 0)
            return;
        std::vector<std::uint8_t> table(size);
        if (GetSystemFirmwareTable(kRawSmbios, 0, table.data(), size) != size || size < 8)
            return;
        std::uint32_t tableLength = 0;
        std::memcpy(&tableLength, table.data() + 4, sizeof(tableLength));
        const auto end     = std::min<std::size_t>(table.size(), 8ULL + tableLength);
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
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &key) ==
        ERROR_SUCCESS)
    {
        wchar_t value[256]{};
        DWORD size = sizeof(value), type = 0;
        if (RegQueryValueExW(key, L"MachineGuid", nullptr, &type, reinterpret_cast<BYTE *>(value), &size) == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ))
        {
            char utf8[768]{};
            const auto chars = std::min<DWORD>(size / sizeof(wchar_t), static_cast<DWORD>(std::size(value) - 1));
            value[chars]     = L'\0';
            const int length =
                WideCharToMultiByte(CP_UTF8, 0, value, static_cast<int>(chars), utf8, sizeof(utf8) - 1, nullptr, nullptr);
            if (length > 1)
                factors.emplace_back("machine_guid", utf8);
        }
        RegCloseKey(key);
    }
    wchar_t windowsDir[MAX_PATH]{};
    wchar_t root[MAX_PATH]{};
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH) != 0 && GetVolumePathNameW(windowsDir, root, MAX_PATH) != 0)
    {
        DWORD serial = 0;
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
auto toJsonFactors(const std::vector<MachineFactor> &factors) -> std::vector<MachineFactorJson>
{
    std::vector<MachineFactorJson> out;
    out.reserve(factors.size());
    for (const auto &factor : factors)
        out.push_back({factor.type, factor.hash});
    return out;
}
auto fromJsonFactors(const std::vector<MachineFactorJson> &factors) -> std::optional<std::vector<MachineFactor>>
{
    if (factors.empty() || factors.size() > kMaxMachineFactors)
        return std::nullopt;
    std::vector<MachineFactor> out;
    for (const auto &factor : factors)
    {
        if (!factor.type || !factor.hash || factor.type->empty() || factor.type->size() > kMaxMachineFactorType ||
            factor.hash->size() != 64 || !hexBytes(*factor.hash) ||
            std::any_of(out.begin(), out.end(), [&](const auto &item) { return item.type == *factor.type; }))
            return std::nullopt;
        out.push_back({*factor.type, *factor.hash});
    }
    return out;
}
template <typename T>
auto serializeJson(const T &value) -> std::string
{
    std::string text;
    SZX::COMMON::Serializer::to_json(value, text);
    return text;
}
template <typename T>
auto parseJson(std::string_view text, T &value) -> bool
{
    std::error_code error;
    SZX::COMMON::Serializer::from_json(value, text, error);
    return !error;
}
auto writeFileAtomic(const std::filesystem::path &path, std::span<const std::uint8_t> bytes) -> Result<void>
{
    std::filesystem::path temporary;
    try
    {
        if (path.empty() || bytes.size() > kMaxFileBytes || SZX::COMMON::FileHelper::Exists(path))
            return fail<void>(LicenseError::InvalidArgument, "output path is invalid or already exists.");
        temporary  = path;
        temporary += ".tmp";
        if (SZX::COMMON::FileHelper::Exists(temporary))
            return fail<void>(LicenseError::IoError, "temporary output already exists.");
        const std::vector<std::uint8_t> copy(bytes.begin(), bytes.end());
        if (!SZX::COMMON::FileHelper::WriteBytes(temporary, copy, false))
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return fail<void>(LicenseError::IoError, "failed to write output file.");
        }
        std::filesystem::rename(temporary, path);
        return Result<void>::Success();
    } catch (...)
    {
        if (!temporary.empty())
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return fail<void>(LicenseError::IoError, "failed to commit output file.");
    }
}
auto readFile(const std::filesystem::path &path) -> Result<std::vector<std::uint8_t>>
{
    try
    {
        if (path.empty() || !SZX::COMMON::FileHelper::IsRegularFile(path))
            return fail<std::vector<std::uint8_t>>(LicenseError::IoError, "input file is unavailable.");
        const auto size = SZX::COMMON::FileHelper::Size(path);
        if (!size)
            return fail<std::vector<std::uint8_t>>(LicenseError::IoError, "failed to inspect input file.");
        if (*size > kMaxFileBytes)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "input file is too large.");
        auto bytes = SZX::COMMON::FileHelper::ReadBytes(path, kMaxFileBytes);
        if (!bytes)
            return fail<std::vector<std::uint8_t>>(LicenseError::IoError, "failed to read input file.");
        if (bytes->size() != *size)
            return fail<std::vector<std::uint8_t>>(LicenseError::IoError, "input file changed while reading.");
        return Result<std::vector<std::uint8_t>>::Success(std::move(*bytes));
    } catch (...)
    {
        return fail<std::vector<std::uint8_t>>(LicenseError::IoError, "failed to read input file.");
    }
}
auto encryptRequest(std::string_view requestJson, std::span<const std::uint8_t> issuerPublicKey)
    -> Result<std::vector<std::uint8_t>>
{
    auto sessionKey = SZX::CRYPTO::randomBytes(32);
    if (!sessionKey.ok)
        return Result<std::vector<std::uint8_t>>::Failure(sessionKey.error, sessionKey.message);
    SZX::CRYPTO_SERVICE::CryptoService crypto;
    auto wrapped = crypto.rsaEncrypt(issuerPublicKey, sessionKey.value);
    if (!wrapped.ok)
        return Result<std::vector<std::uint8_t>>::Failure(wrapped.error, wrapped.message);
    std::vector<std::uint8_t> header{'S', 'Z', 'X', 'R'};
    put16(header, kRequestVersion);
    put16(header, kRequestAlgorithm);
    put32(header, static_cast<std::uint32_t>(wrapped.value.size()));
    put32(header, static_cast<std::uint32_t>(kRequestNonceSize));
    put32(header, static_cast<std::uint32_t>(kRequestTagSize));
    put32(header, static_cast<std::uint32_t>(requestJson.size()));
    auto encrypted = crypto.encrypt(
        sessionKey.value,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(requestJson.data()), requestJson.size()), header);
    if (!encrypted.ok)
        return Result<std::vector<std::uint8_t>>::Failure(encrypted.error, encrypted.message);
    if (encrypted.value.nonce.size() != kRequestNonceSize || encrypted.value.tag.size() != kRequestTagSize ||
        encrypted.value.ciphertext.size() != requestJson.size())
        return fail<std::vector<std::uint8_t>>(LicenseError::Unsupported, "unexpected request cipher format.");
    header.insert(header.end(), wrapped.value.begin(), wrapped.value.end());
    header.insert(header.end(), encrypted.value.nonce.begin(), encrypted.value.nonce.end());
    header.insert(header.end(), encrypted.value.tag.begin(), encrypted.value.tag.end());
    header.insert(header.end(), encrypted.value.ciphertext.begin(), encrypted.value.ciphertext.end());
    return Result<std::vector<std::uint8_t>>::Success(std::move(header));
}
auto decryptRequest(std::span<const std::uint8_t> envelope, std::span<const std::uint8_t> issuerPrivateKey) -> Result<std::string>
{
    if (envelope.size() < 24 || !std::equal(envelope.begin(), envelope.begin() + 4, "SZXR"))
        return fail<std::string>(LicenseError::Malformed, "invalid request envelope.");
    std::vector<std::uint8_t> encoded(envelope.begin(), envelope.end());
    std::vector<std::uint8_t> header(encoded.begin(), encoded.begin() + 24);
    std::size_t p        = 8;
    auto wrappedSize     = read32(encoded, p);
    auto nonceSize       = read32(encoded, p);
    auto tagSize         = read32(encoded, p);
    auto cipherSize      = read32(encoded, p);
    const auto version   = static_cast<std::uint16_t>(header[4] | (header[5] << 8));
    const auto algorithm = static_cast<std::uint16_t>(header[6] | (header[7] << 8));
    if (version != kRequestVersion || algorithm != kRequestAlgorithm)
        return fail<std::string>(LicenseError::Unsupported, "unsupported request envelope.");
    if (!wrappedSize || !nonceSize || !tagSize || !cipherSize || *wrappedSize == 0 || *nonceSize == 0 || *tagSize == 0 ||
        *cipherSize > kMaxPayload || p > envelope.size() || *wrappedSize > envelope.size() - p ||
        *nonceSize > envelope.size() - p - *wrappedSize || *tagSize > envelope.size() - p - *wrappedSize - *nonceSize ||
        *cipherSize != envelope.size() - p - *wrappedSize - *nonceSize - *tagSize)
        return fail<std::string>(LicenseError::Malformed, "invalid request envelope lengths.");
    SZX::CRYPTO_SERVICE::CryptoService crypto;
    auto key = crypto.rsaDecrypt(issuerPrivateKey, envelope.subspan(p, *wrappedSize));
    if (!key.ok)
        return Result<std::string>::Failure(key.error, key.message);
    p += *wrappedSize;
    SZX::CRYPTO_SERVICE::CryptoCiphertext encrypted;
    encrypted.nonce.assign(envelope.begin() + p, envelope.begin() + p + *nonceSize);
    p += *nonceSize;
    encrypted.tag.assign(envelope.begin() + p, envelope.begin() + p + *tagSize);
    p += *tagSize;
    encrypted.ciphertext.assign(envelope.begin() + p, envelope.end());
    auto plain = crypto.decrypt(key.value, encrypted, header);
    if (!plain.ok)
        return Result<std::string>::Failure(plain.error, plain.message);
    return Result<std::string>::Success(std::string(plain.value.begin(), plain.value.end()));
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
                                                 "SZX-LICENSE-MACHINE-V1|" + std::string(productId) + "|" + normalizedType + "|" +
                                                     normalizedRaw);
            if (!hashed.ok)
                return Result<std::vector<MachineFactor>>::Failure(hashed.error, hashed.message);
            out.push_back({normalizedType, std::move(hashed.value)});
        }
        if (out.empty())
            return fail<std::vector<MachineFactor>>(LicenseError::Unsupported, "machine identifiers are unavailable.");
        return Result<std::vector<MachineFactor>>::Success(std::move(out));
    } catch (...)
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
        if (!fromJsonFactors(toJsonFactors(machineFactors.value)))
            return fail<std::string>(LicenseError::InvalidArgument, "invalid machine factors.");
        RequestDocument document;
        document.schema_version     = 1;
        document.request_id         = encoded.value;
        document.product_id         = options.productId;
        document.app_version        = options.appVersion;
        document.created_at         = nowUtc();
        document.nonce              = encoded.value;
        document.machine_factors    = toJsonFactors(machineFactors.value);
        document.requested_features = options.requestedFeatures;
        document.custom_fields      = options.customFields;
        auto text                   = serializeJson(document);
        if (text.size() > kMaxRequest)
            return fail<std::string>(LicenseError::InvalidArgument, "request payload too large.");
        return Result<std::string>::Success(std::move(text));
    } catch (...)
    {
        return fail<std::string>(LicenseError::Malformed, "request generation failed.");
    }
}

auto createRequestFile(const RequestOptions &options, std::string_view outputPath, std::span<const std::uint8_t> issuerPublicKey)
    -> Result<void>
{
    try
    {
        if (issuerPublicKey.empty())
            return fail<void>(LicenseError::InvalidArgument, "issuer public key is required.");
        auto request = createRequest(options);
        if (!request.ok)
            return Result<void>::Failure(request.error, request.message);
        auto encrypted = encryptRequest(request.value, issuerPublicKey);
        if (!encrypted.ok)
            return Result<void>::Failure(encrypted.error, encrypted.message);
        return writeFileAtomic(std::filesystem::path(outputPath), encrypted.value);
    } catch (...)
    {
        return fail<void>(LicenseError::IoError, "failed to create request file.");
    }
}

auto issue(const std::string &requestJson, const IssueOptions &options, std::span<const std::uint8_t> issuerPrivateKey,
           std::span<const std::uint8_t> issuerPublicKey) -> Result<std::vector<std::uint8_t>>
{
    try
    {
        if (requestJson.empty() || requestJson.size() > kMaxRequest)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "request payload size invalid.");
        RequestDocument req;
        if (!parseJson(requestJson, req))
            return fail<std::vector<std::uint8_t>>(LicenseError::Malformed, "invalid request JSON.");
        if (!req.schema_version || *req.schema_version != 1)
            return fail<std::vector<std::uint8_t>>(LicenseError::Unsupported, "unsupported request format.");
        if (!req.request_id || !req.product_id || !req.app_version || !req.created_at || !req.nonce || !req.machine_factors ||
            !req.requested_features || !req.custom_fields)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "request fields are required.");
        if (req.request_id->empty() || req.product_id->empty() || req.app_version->empty() || req.nonce->empty())
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "request fields are invalid.");
        const auto product         = *req.product_id;
        const auto version         = *req.app_version;
        auto requestMachineFactors = fromJsonFactors(*req.machine_factors);
        if (!requestMachineFactors)
            return fail<std::vector<std::uint8_t>>(LicenseError::InvalidArgument, "machine factors are required.");
        if (product.empty() || version.empty() || options.licenseId.empty() || options.issuer.empty() ||
            issuerPrivateKey.empty() || issuerPublicKey.empty())
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
        const auto features = options.features.empty() ? *req.requested_features : options.features;
        LicensePayload payload;
        payload.schema_version  = 1;
        payload.license_id      = options.licenseId;
        payload.product_id      = product;
        payload.app_version     = version;
        payload.issuer          = options.issuer;
        payload.issued_at       = issued;
        payload.not_before      = notBefore;
        payload.expires_at      = options.expiresAtUtc;
        payload.features        = features;
        payload.machine_factors = toJsonFactors(*requestMachineFactors);
        payload.custom_fields   = options.customFields;
        const auto text         = serializeJson(payload);
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

auto issueFile(std::string_view requestPath, const IssueOptions &options, std::span<const std::uint8_t> issuerPrivateKey,
               std::span<const std::uint8_t> issuerPublicKey) -> Result<std::vector<std::uint8_t>>
{
    try
    {
        auto envelope = readFile(std::filesystem::path(requestPath));
        if (!envelope.ok)
            return Result<std::vector<std::uint8_t>>::Failure(envelope.error, envelope.message);
        auto request = decryptRequest(envelope.value, issuerPrivateKey);
        if (!request.ok)
            return Result<std::vector<std::uint8_t>>::Failure(request.error, request.message);
        return issue(request.value, options, issuerPrivateKey, issuerPublicKey);
    } catch (...)
    {
        return fail<std::vector<std::uint8_t>>(LicenseError::IoError, "failed to issue request file.");
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
        LicensePayload payload;
        // `expires_at` is a required member; a null value represents a perpetual license.
        // Iguana maps both a missing optional member and JSON null to an empty optional, so
        // retain an explicit presence check before consuming the DTO.
        if (text.find("\"expires_at\"") == std::string_view::npos || !parseJson(text, payload) || !payload.schema_version ||
            *payload.schema_version != 1 ||
            !payload.issuer || !payload.license_id || !payload.product_id || !payload.app_version ||
            !payload.issued_at || !payload.not_before || !payload.features || !payload.machine_factors ||
            !payload.custom_fields)
            return fail<LicenseInfo>(LicenseError::Malformed, "license payload fields invalid.");
        auto machineFactors = fromJsonFactors(*payload.machine_factors);
        if (!machineFactors)
            return fail<LicenseInfo>(LicenseError::Malformed, "license machine factors invalid.");
        LicenseInfo out{*payload.license_id,
                        *payload.product_id,
                        *payload.app_version,
                        *payload.issuer,
                        *payload.issued_at,
                        *payload.not_before,
                        std::nullopt,
                        *payload.features,
                        *machineFactors,
                        *payload.custom_fields};
        if (payload.expires_at)
            out.expiresAtUtc = *payload.expires_at;
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
                if (std::any_of(local.value.begin(), local.value.end(), [&](const auto &actual)
                                { return actual.type == expected.type && actual.hash == expected.hash; }) &&
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

auto verifyFile(std::string_view licensePath, std::span<const std::uint8_t> issuerPublicKey, std::string_view expectedProduct)
    -> Result<LicenseInfo>
{
    try
    {
        auto bytes = readFile(std::filesystem::path(licensePath));
        if (!bytes.ok)
            return Result<LicenseInfo>::Failure(bytes.error, bytes.message);
        return verify(bytes.value, issuerPublicKey, expectedProduct);
    } catch (...)
    {
        return fail<LicenseInfo>(LicenseError::IoError, "failed to verify license file.");
    }
}

auto hasFeature(const LicenseInfo &license, std::string_view feature) noexcept -> bool
{
    return std::find(license.features.begin(), license.features.end(), feature) != license.features.end();
}
} // namespace SZX::LICENSE
