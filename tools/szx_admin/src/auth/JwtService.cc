#include "SzxAdmin/Auth/JwtService.h"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <span>
#include <system_error>
#include <utility>

namespace SZX::ADMIN::AUTH
{
namespace
{
using CryptoService = SZX::CRYPTO_SERVICE::ICryptoService;

struct Ps256ServiceAlgorithm
{
    CryptoService &crypto;
    std::vector<std::uint8_t> key;
    bool signing{};

    ~Ps256ServiceAlgorithm() noexcept
    {
        std::fill(key.begin(), key.end(), std::uint8_t{});
    }

    [[nodiscard]] auto sign(const std::string &data, std::error_code &error) const -> std::string
    {
        error.clear();
        if (!signing)
        {
            error = std::make_error_code(std::errc::operation_not_permitted);
            return {};
        }
        const auto result = crypto.signPs256(
            key, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
        if (!result.ok)
        {
            error = std::make_error_code(std::errc::io_error);
            return {};
        }
        return {reinterpret_cast<const char *>(result.value.data()), result.value.size()};
    }

    void verify(const std::string &data, const std::string &signature, std::error_code &error) const
    {
        error.clear();
        const auto result = crypto.verifyPs256(
            key, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()),
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(signature.data()), signature.size()));
        if (!result.ok)
            error = std::make_error_code(std::errc::io_error);
        else if (!result.value)
            error = std::make_error_code(std::errc::permission_denied);
    }

    [[nodiscard]] auto name() const -> std::string
    {
        return "PS256";
    }
};

auto toSeconds(jwt::date value) -> std::int64_t
{
    return std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch()).count();
}

auto failure(JwtError error, const char *message) -> SZX::COMMON::Result<std::string>
{
    return SZX::COMMON::Result<std::string>::Failure(static_cast<int>(error), message);
}

auto validConfig(const JwtConfig &config) -> bool
{
    return !config.issuer.empty() && !config.audience.empty() && config.lifetimeSeconds > 0 && config.clockLeewaySeconds >= 0;
}

auto validKey(const JwtSigningKey &key, bool signing) -> bool
{
    return !key.keyId.empty() && (signing ? !key.privateKey.empty() : !key.publicKey.empty());
}

auto currentSeconds() -> std::int64_t
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

auto effectiveSeconds(std::int64_t value) -> std::int64_t
{
    return value == 0 ? currentSeconds() : value;
}

} // namespace

JwtKeyStore::JwtKeyStore(SZX::CRYPTO_SERVICE::ICryptoService &crypto) :
    crypto_(crypto)
{
}

auto JwtKeyStore::addKey(JwtKeyRecord record) -> SZX::COMMON::Result<void>
{
    try
    {
        if (record.key.keyId.empty() || record.key.publicKey.empty() ||
            (record.activeForSigning && record.key.privateKey.empty()) || record.notBefore < 0 || record.expiresAt < 0 ||
            (record.expiresAt != 0 && record.expiresAt < record.notBefore))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InvalidArgument), "Invalid JWT key record.");
        if (record.activeForSigning)
        {
            const auto timestamp = currentSeconds();
            if (timestamp < record.notBefore || (record.expiresAt != 0 && timestamp > record.expiresAt))
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected),
                                                          "The JWT signing key is not currently valid.");
        }

        std::unique_lock lock(mutex_);
        if (keys_.contains(record.key.keyId))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "The JWT key ID already exists.");
        const auto publicKeyId = crypto_.publicKeyId(record.key.publicKey);
        if (!publicKeyId.ok)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::CryptoFailure), "Failed to calculate the JWT public key ID.");
        if (publicKeyId.value != record.key.keyId)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "The JWT key ID does not match the public key.");
        const auto inserted = keys_.emplace(record.key.keyId, std::move(record));
        if (!inserted.second)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "The JWT key ID already exists.");
        if (inserted.first->second.activeForSigning)
        {
            for (auto &[keyId, existing] : keys_)
                if (keyId != inserted.first->first)
                    existing.activeForSigning = false;
        }
        return SZX::COMMON::Result<void>::Success();
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT key storage failed.");
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT key storage failed.");
    }
}

auto JwtKeyStore::activateKey(std::string_view keyId, std::int64_t now) -> SZX::COMMON::Result<void>
{
    try
    {
        std::unique_lock lock(mutex_);
        const auto found = keys_.find(keyId);
        if (found == keys_.end() || found->second.key.privateKey.empty())
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "The JWT signing key does not exist.");
        const auto timestamp = effectiveSeconds(now);
        if (timestamp < found->second.notBefore || (found->second.expiresAt != 0 && timestamp > found->second.expiresAt))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "The JWT signing key is not valid.");
        for (auto &[currentId, record] : keys_)
            record.activeForSigning = currentId == keyId;
        return SZX::COMMON::Result<void>::Success();
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT key rotation failed.");
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT key rotation failed.");
    }
}

auto JwtKeyStore::signingKey(std::int64_t now) const -> SZX::COMMON::Result<JwtSigningKey>
{
    try
    {
        std::shared_lock lock(mutex_);
        const auto timestamp = effectiveSeconds(now);
        for (const auto &[keyId, record] : keys_)
        {
            if (record.activeForSigning && timestamp >= record.notBefore &&
                (record.expiresAt == 0 || timestamp <= record.expiresAt))
                return SZX::COMMON::Result<JwtSigningKey>::Success(record.key);
        }
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::KeyRejected), "No usable JWT signing key is available.");
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "Failed to read the JWT signing key.");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "Failed to read the JWT signing key.");
    }
}

auto JwtKeyStore::verificationKey(std::string_view keyId, std::int64_t now) const -> SZX::COMMON::Result<JwtSigningKey>
{
    try
    {
        std::shared_lock lock(mutex_);
        const auto found     = keys_.find(keyId);
        const auto timestamp = effectiveSeconds(now);
        if (found == keys_.end() || timestamp < found->second.notBefore ||
            (found->second.expiresAt != 0 && timestamp > found->second.expiresAt))
            return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::KeyRejected),
                                                               "The JWT verification key does not exist or has expired.");
        auto verificationKey = found->second.key;
        verificationKey.privateKey.clear();
        return SZX::COMMON::Result<JwtSigningKey>::Success(std::move(verificationKey));
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "Failed to read the JWT verification key.");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "Failed to read the JWT verification key.");
    }
}

JwtService::JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config) :
    crypto_(crypto),
    config_(std::move(config))
{
}

JwtService::JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config, JwtKeyStore &keyStore) :
    crypto_(crypto),
    config_(std::move(config)),
    keyStore_(&keyStore)
{
}

auto JwtService::issue(const JwtIssueRequest &request) -> SZX::COMMON::Result<std::string>
{
    if (keyStore_ == nullptr)
        return failure(JwtError::KeyRejected, "JWT key storage is not configured.");
    try
    {
        const auto key = keyStore_->signingKey();
        if (!key.ok)
            return SZX::COMMON::Result<std::string>::Failure(key.error, key.message);
        return issue(request, key.value);
    } catch (const std::exception &)
    {
        return failure(JwtError::InternalFailure, "Internal JWT service exception.");
    } catch (...)
    {
        return failure(JwtError::InternalFailure, "Internal JWT service exception.");
    }
}

auto JwtService::issue(const JwtIssueRequest &request, const JwtSigningKey &key) -> SZX::COMMON::Result<std::string>
{
    try
    {
        if (!validConfig(config_) || request.subject.empty() || request.jti.empty() || request.roleId.empty() || !validKey(key, true))
            return failure(JwtError::InvalidArgument, "Invalid JWT request or configuration.");

        const auto now      = std::chrono::system_clock::now();
        const auto issuedAt = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        if (request.authzVersion > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) ||
            request.roleAuthzVersion > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) ||
            config_.lifetimeSeconds > (std::numeric_limits<std::int64_t>::max)() - issuedAt)
            return failure(JwtError::InvalidArgument, "JWT numeric value is out of range.");
        const auto expiresAt   = issuedAt + config_.lifetimeSeconds;
        const auto issuedDate  = jwt::date{std::chrono::system_clock::time_point{std::chrono::seconds{issuedAt}}};
        const auto expiresDate = jwt::date{std::chrono::system_clock::time_point{std::chrono::seconds{expiresAt}}};

        auto builder = jwt::create()
                           .set_type("JWT")
                           .set_algorithm("PS256")
                           .set_key_id(key.keyId)
                           .set_issuer(config_.issuer)
                           .set_audience(config_.audience)
                           .set_subject(request.subject)
                           .set_id(request.jti)
                           .set_issued_at(issuedDate)
                           .set_not_before(issuedDate)
                           .set_expires_at(expiresDate)
                           .set_payload_claim("authz_version",
                                              jwt::claim(jwt::traits::kazuho_picojson::value_type(
                                                  jwt::traits::kazuho_picojson::integer_type(request.authzVersion))))
                           .set_payload_claim("role_id", jwt::claim(request.roleId))
                           .set_payload_claim("role_authz_version",
                                              jwt::claim(jwt::traits::kazuho_picojson::value_type(
                                                  jwt::traits::kazuho_picojson::integer_type(request.roleAuthzVersion))));

        std::error_code error;
        const auto token = builder.sign(Ps256ServiceAlgorithm{crypto_, key.privateKey, true}, error);
        if (error)
            return failure(JwtError::CryptoFailure, "JWT signing failed.");
        return SZX::COMMON::Result<std::string>::Success(token);
    } catch (const std::exception &)
    {
        return failure(JwtError::InternalFailure, "Internal JWT service exception.");
    } catch (...)
    {
        return failure(JwtError::InternalFailure, "Internal JWT service exception.");
    }
}

auto JwtService::verify(std::string_view token, const JwtSigningKey &key) -> SZX::COMMON::Result<JwtClaims>
{
    try
    {
        if (!validConfig(config_) || token.empty() || !validKey(key, false))
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidArgument), "Invalid JWT request or configuration.");

        const auto decoded = jwt::decode(std::string{token});
        if (decoded.get_algorithm() != "PS256")
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::AlgorithmRejected), "The JWT algorithm is not allowed.");
        if (!decoded.has_key_id() || decoded.get_key_id() != key.keyId)
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::KeyRejected), "Invalid JWT key ID.");

        auto verifier = jwt::verify()
                            .allow_algorithm(Ps256ServiceAlgorithm{crypto_, key.publicKey, false})
                            .with_issuer(config_.issuer)
                            .with_audience(config_.audience)
                            .leeway(static_cast<std::size_t>(config_.clockLeewaySeconds));
        std::error_code error;
        verifier.verify(decoded, error);
        if (error)
        {
            const auto signatureError = std::make_error_code(std::errc::permission_denied);
            const auto cryptoError    = std::make_error_code(std::errc::io_error);
            const auto jwtError       = error == signatureError ? JwtError::SignatureInvalid :
                      error == cryptoError                      ? JwtError::CryptoFailure :
                                                                  JwtError::ClaimsInvalid;
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(jwtError),
                                                           jwtError == JwtError::SignatureInvalid  ? "Invalid JWT signature." :
                                                               jwtError == JwtError::CryptoFailure ? "JWT cryptographic verification failed." :
                                                                                                     "Invalid JWT claims.");
        }

        if (!decoded.has_subject() || !decoded.has_id() || !decoded.has_issued_at() || !decoded.has_not_before() ||
            !decoded.has_expires_at() || !decoded.has_payload_claim("authz_version") ||
            !decoded.has_payload_claim("role_id") || !decoded.has_payload_claim("role_authz_version"))
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::ClaimsInvalid), "JWT is missing required claims.");

        const auto authzVersion = decoded.get_payload_claim("authz_version").as_integer();
        const auto roleAuthzVersion = decoded.get_payload_claim("role_authz_version").as_integer();
        if (authzVersion < 0 || roleAuthzVersion < 0 || decoded.get_payload_claim("role_id").as_string().empty())
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::ClaimsInvalid), "Invalid JWT authorization version.");

        JwtClaims claims{.keyId        = decoded.get_key_id(),
                         .issuer       = decoded.get_issuer(),
                         .audience     = config_.audience,
                         .subject      = decoded.get_subject(),
                         .jti          = decoded.get_id(),
                         .issuedAt     = toSeconds(decoded.get_issued_at()),
                         .notBefore    = toSeconds(decoded.get_not_before()),
                         .expiresAt        = toSeconds(decoded.get_expires_at()),
                         .authzVersion     = static_cast<std::uint64_t>(authzVersion),
                         .roleId           = decoded.get_payload_claim("role_id").as_string(),
                         .roleAuthzVersion = static_cast<std::uint64_t>(roleAuthzVersion)};
        return SZX::COMMON::Result<JwtClaims>::Success(std::move(claims));
    } catch (const std::bad_alloc &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "Internal JWT service exception.");
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidToken), "Invalid JWT format or content.");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "Internal JWT service exception.");
    }
}

auto JwtService::verify(std::string_view token) -> SZX::COMMON::Result<JwtClaims>
{
    if (keyStore_ == nullptr)
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT key storage is not configured.");

    try
    {
        const auto decoded = jwt::decode(std::string{token});
        if (!decoded.has_key_id())
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT is missing a key ID.");

        const auto key = keyStore_->verificationKey(decoded.get_key_id());
        if (!key.ok)
            return SZX::COMMON::Result<JwtClaims>::Failure(key.error, key.message);
        return verify(token, key.value);
    } catch (const std::bad_alloc &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "Internal JWT service exception.");
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidToken), "Invalid JWT format or content.");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "Internal JWT service exception.");
    }
}
} // namespace SZX::ADMIN::AUTH
