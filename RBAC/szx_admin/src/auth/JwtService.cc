#include "SzxAdmin/Auth/JwtService.h"

#include <jwt-cpp/jwt.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
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

auto environmentValue(const char *name) -> std::string
{
#if defined(_WIN32)
    char *value       = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
        return {};
    std::string result{value};
    std::free(value);
    return result;
#else
    const auto *value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

auto parseEnvironmentInteger(const char *name, std::int64_t defaultValue, bool positive) -> SZX::COMMON::Result<std::int64_t>
{
    const auto text = environmentValue(name);
    if (text.empty())
        return SZX::COMMON::Result<std::int64_t>::Success(defaultValue);

    std::int64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || (positive ? value <= 0 : value < 0))
        return SZX::COMMON::Result<std::int64_t>::Failure(static_cast<int>(JwtError::InvalidArgument), "JWT 配置数值无效。");
    return SZX::COMMON::Result<std::int64_t>::Success(value);
}
} // namespace

JwtKeyStore::JwtKeyStore(SZX::CRYPTO_SERVICE::ICryptoService &crypto) :
    crypto_(crypto)
{
}

auto JwtConfig::loadFromEnvironment() -> SZX::COMMON::Result<JwtConfig>
{
    try
    {
        JwtConfig config{.issuer             = environmentValue("RBAC_JWT_ISSUER"),
                         .audience           = environmentValue("RBAC_JWT_AUDIENCE"),
                         .lifetimeSeconds    = 900,
                         .clockLeewaySeconds = 30};
        if (config.issuer.empty() || config.audience.empty())
            return SZX::COMMON::Result<JwtConfig>::Failure(static_cast<int>(JwtError::InvalidArgument),
                                                           "JWT issuer/audience 未配置。");

        const auto lifetime = parseEnvironmentInteger("RBAC_JWT_LIFETIME_SECONDS", config.lifetimeSeconds, true);
        if (!lifetime.ok)
            return SZX::COMMON::Result<JwtConfig>::Failure(lifetime.error, lifetime.message);
        const auto leeway = parseEnvironmentInteger("RBAC_JWT_CLOCK_LEEWAY_SECONDS", config.clockLeewaySeconds, false);
        if (!leeway.ok)
            return SZX::COMMON::Result<JwtConfig>::Failure(leeway.error, leeway.message);
        config.lifetimeSeconds    = lifetime.value;
        config.clockLeewaySeconds = leeway.value;
        return SZX::COMMON::Result<JwtConfig>::Success(std::move(config));
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtConfig>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 配置加载失败。");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtConfig>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 配置加载失败。");
    }
}

auto JwtKeyStore::addKey(JwtKeyRecord record) -> SZX::COMMON::Result<void>
{
    try
    {
        if (record.key.keyId.empty() || record.key.publicKey.empty() ||
            (record.activeForSigning && record.key.privateKey.empty()) || record.notBefore < 0 || record.expiresAt < 0 ||
            (record.expiresAt != 0 && record.expiresAt < record.notBefore))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InvalidArgument), "JWT 密钥记录无效。");
        if (record.activeForSigning)
        {
            const auto timestamp = currentSeconds();
            if (timestamp < record.notBefore || (record.expiresAt != 0 && timestamp > record.expiresAt))
                return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected),
                                                          "JWT 签名密钥当前不在有效期内。");
        }

        std::unique_lock lock(mutex_);
        if (keys_.contains(record.key.keyId))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 密钥标识已存在。");
        const auto publicKeyId = crypto_.publicKeyId(record.key.publicKey);
        if (!publicKeyId.ok)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::CryptoFailure), "JWT 公钥标识计算失败。");
        if (publicKeyId.value != record.key.keyId)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 密钥标识与公钥不匹配。");
        const auto inserted = keys_.emplace(record.key.keyId, std::move(record));
        if (!inserted.second)
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 密钥标识已存在。");
        if (inserted.first->second.activeForSigning)
        {
            for (auto &[keyId, existing] : keys_)
                if (keyId != inserted.first->first)
                    existing.activeForSigning = false;
        }
        return SZX::COMMON::Result<void>::Success();
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 密钥存储失败。");
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 密钥存储失败。");
    }
}

auto JwtKeyStore::activateKey(std::string_view keyId, std::int64_t now) -> SZX::COMMON::Result<void>
{
    try
    {
        std::unique_lock lock(mutex_);
        const auto found = keys_.find(keyId);
        if (found == keys_.end() || found->second.key.privateKey.empty())
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 签名密钥不存在。");
        const auto timestamp = effectiveSeconds(now);
        if (timestamp < found->second.notBefore || (found->second.expiresAt != 0 && timestamp > found->second.expiresAt))
            return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 签名密钥不在有效期内。");
        for (auto &[currentId, record] : keys_)
            record.activeForSigning = currentId == keyId;
        return SZX::COMMON::Result<void>::Success();
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 密钥轮换失败。");
    } catch (...)
    {
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 密钥轮换失败。");
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
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::KeyRejected), "没有可用的 JWT 签名密钥。");
    }
    catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 签名密钥读取失败。");
    }
    catch (...)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 签名密钥读取失败。");
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
                                                               "JWT 验证密钥不存在或已过期。");
        auto verificationKey = found->second.key;
        verificationKey.privateKey.clear();
        return SZX::COMMON::Result<JwtSigningKey>::Success(std::move(verificationKey));
    }
    catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 验证密钥读取失败。");
    }
    catch (...)
    {
        return SZX::COMMON::Result<JwtSigningKey>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 验证密钥读取失败。");
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
        return failure(JwtError::KeyRejected, "JWT 密钥存储未配置。");
    try
    {
        const auto key = keyStore_->signingKey();
        if (!key.ok)
            return SZX::COMMON::Result<std::string>::Failure(key.error, key.message);
        return issue(request, key.value);
    }
    catch (const std::exception &)
    {
        return failure(JwtError::InternalFailure, "JWT 服务内部异常。");
    }
    catch (...)
    {
        return failure(JwtError::InternalFailure, "JWT 服务内部异常。");
    }
}

auto JwtService::issue(const JwtIssueRequest &request, const JwtSigningKey &key) -> SZX::COMMON::Result<std::string>
{
    try
    {
        if (!validConfig(config_) || request.subject.empty() || request.jti.empty() || !validKey(key, true))
            return failure(JwtError::InvalidArgument, "JWT 请求或配置无效。");

        const auto now      = std::chrono::system_clock::now();
        const auto issuedAt = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        if (request.authzVersion > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) ||
            config_.lifetimeSeconds > (std::numeric_limits<std::int64_t>::max)() - issuedAt)
            return failure(JwtError::InvalidArgument, "JWT 数值超出允许范围。");
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
                                                  jwt::traits::kazuho_picojson::integer_type(request.authzVersion))));

        std::error_code error;
        const auto token = builder.sign(Ps256ServiceAlgorithm{crypto_, key.privateKey, true}, error);
        if (error)
            return failure(JwtError::CryptoFailure, "JWT 签名失败。");
        return SZX::COMMON::Result<std::string>::Success(token);
    } catch (const std::exception &)
    {
        return failure(JwtError::InternalFailure, "JWT 服务内部异常。");
    } catch (...)
    {
        return failure(JwtError::InternalFailure, "JWT 服务内部异常。");
    }
}

auto JwtService::verify(std::string_view token, const JwtSigningKey &key) -> SZX::COMMON::Result<JwtClaims>
{
    try
    {
        if (!validConfig(config_) || token.empty() || !validKey(key, false))
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidArgument), "JWT 请求或配置无效。");

        const auto decoded = jwt::decode(std::string{token});
        if (decoded.get_algorithm() != "PS256")
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::AlgorithmRejected), "JWT 算法不被允许。");
        if (!decoded.has_key_id() || decoded.get_key_id() != key.keyId)
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 密钥标识无效。");

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
                error == cryptoError                            ? JwtError::CryptoFailure :
                                                                  JwtError::ClaimsInvalid;
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(jwtError),
                                                           jwtError == JwtError::SignatureInvalid  ? "JWT 签名无效。" :
                                                               jwtError == JwtError::CryptoFailure ? "JWT 验证密码学操作失败。" :
                                                                                                     "JWT 声明无效。");
        }

        if (!decoded.has_subject() || !decoded.has_id() || !decoded.has_issued_at() || !decoded.has_not_before() ||
            !decoded.has_expires_at() || !decoded.has_payload_claim("authz_version"))
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::ClaimsInvalid), "JWT 缺少必要声明。");

        const auto authzVersion = decoded.get_payload_claim("authz_version").as_integer();
        if (authzVersion < 0)
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::ClaimsInvalid), "JWT 授权版本无效。");

        JwtClaims claims{.keyId        = decoded.get_key_id(),
                         .issuer       = decoded.get_issuer(),
                         .audience     = config_.audience,
                         .subject      = decoded.get_subject(),
                         .jti          = decoded.get_id(),
                         .issuedAt     = toSeconds(decoded.get_issued_at()),
                         .notBefore    = toSeconds(decoded.get_not_before()),
                         .expiresAt    = toSeconds(decoded.get_expires_at()),
                         .authzVersion = static_cast<std::uint64_t>(authzVersion)};
        return SZX::COMMON::Result<JwtClaims>::Success(std::move(claims));
    } catch (const std::bad_alloc &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 服务内部异常。");
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidToken), "JWT 格式或内容无效。");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 服务内部异常。");
    }
}

auto JwtService::verify(std::string_view token) -> SZX::COMMON::Result<JwtClaims>
{
    if (keyStore_ == nullptr)
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 密钥存储未配置。");

    try
    {
        const auto decoded = jwt::decode(std::string{token});
        if (!decoded.has_key_id())
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::KeyRejected), "JWT 缺少密钥标识。");

        const auto key = keyStore_->verificationKey(decoded.get_key_id());
        if (!key.ok)
            return SZX::COMMON::Result<JwtClaims>::Failure(key.error, key.message);
        return verify(token, key.value);
    } catch (const std::bad_alloc &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 服务内部异常。");
    } catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidToken), "JWT 格式或内容无效。");
    } catch (...)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 服务内部异常。");
    }
}
} // namespace SZX::ADMIN::AUTH
