#include "SzxAdmin/Auth/JwtService.h"

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <exception>
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
            key,
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()),
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(signature.data()), signature.size()));
        if (!result.ok || !result.value)
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
    return !config.issuer.empty() && !config.audience.empty() && config.lifetimeSeconds > 0 &&
           config.clockLeewaySeconds >= 0;
}

auto validKey(const JwtSigningKey &key, bool signing) -> bool
{
    return !key.keyId.empty() && (signing ? !key.privateKey.empty() : !key.publicKey.empty());
}
} // namespace

JwtService::JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config)
    : crypto_(crypto), config_(std::move(config))
{
}

auto JwtService::issue(const JwtIssueRequest &request, const JwtSigningKey &key)
    -> SZX::COMMON::Result<std::string>
{
    try
    {
        if (!validConfig(config_) || request.subject.empty() || request.jti.empty() || !validKey(key, true))
            return failure(JwtError::InvalidArgument, "JWT 请求或配置无效。");

        const auto now       = std::chrono::system_clock::now();
        const auto issuedAt  = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        const auto expiresAt = issuedAt + config_.lifetimeSeconds;
        const auto issuedDate = jwt::date{std::chrono::system_clock::time_point{std::chrono::seconds{issuedAt}}};
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
                           .set_payload_claim(
                               "authz_version",
                               jwt::claim(jwt::traits::kazuho_picojson::value_type(
                                   jwt::traits::kazuho_picojson::integer_type(request.authzVersion))));

        std::error_code error;
        const auto token = builder.sign(Ps256ServiceAlgorithm{crypto_, key.privateKey, true}, error);
        if (error)
            return failure(JwtError::CryptoFailure, "JWT 签名失败。");
        return SZX::COMMON::Result<std::string>::Success(token);
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

auto JwtService::verify(std::string_view token, const JwtSigningKey &key)
    -> SZX::COMMON::Result<JwtClaims>
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
            return SZX::COMMON::Result<JwtClaims>::Failure(
                static_cast<int>(error == signatureError ? JwtError::SignatureInvalid : JwtError::ClaimsInvalid),
                error == signatureError ? "JWT 签名无效。" : "JWT 声明无效。");
        }

        if (!decoded.has_subject() || !decoded.has_id() || !decoded.has_issued_at() || !decoded.has_not_before() ||
            !decoded.has_expires_at() || !decoded.has_payload_claim("authz_version"))
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::ClaimsInvalid), "JWT 缺少必要声明。");

        const auto authzVersion = decoded.get_payload_claim("authz_version").as_integer();
        if (authzVersion < 0)
            return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::ClaimsInvalid), "JWT 授权版本无效。");

        JwtClaims claims{.keyId         = decoded.get_key_id(),
                         .issuer        = decoded.get_issuer(),
                         .audience      = config_.audience,
                         .subject       = decoded.get_subject(),
                         .jti           = decoded.get_id(),
                         .issuedAt      = toSeconds(decoded.get_issued_at()),
                         .notBefore     = toSeconds(decoded.get_not_before()),
                         .expiresAt     = toSeconds(decoded.get_expires_at()),
                         .authzVersion  = static_cast<std::uint64_t>(authzVersion)};
        return SZX::COMMON::Result<JwtClaims>::Success(std::move(claims));
    }
    catch (const std::exception &)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InvalidToken), "JWT 格式或内容无效。");
    }
    catch (...)
    {
        return SZX::COMMON::Result<JwtClaims>::Failure(static_cast<int>(JwtError::InternalFailure), "JWT 服务内部异常。");
    }
}
} // namespace SZX::ADMIN::AUTH
