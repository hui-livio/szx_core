#pragma once

#include "SzxCryptoService/CryptoService.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SZX::ADMIN::AUTH
{
enum class JwtError : int
{
    InvalidArgument   = 2100,
    InvalidToken      = 2101,
    AlgorithmRejected = 2102,
    KeyRejected       = 2103,
    SignatureInvalid  = 2104,
    ClaimsInvalid     = 2105,
    CryptoFailure     = 2106,
    InternalFailure   = 2107,
};

struct JwtConfig
{
    std::string issuer;
    std::string audience;
    std::int64_t lifetimeSeconds{900};
    std::int64_t clockLeewaySeconds{30};
};

struct JwtSigningKey
{
    std::string keyId;
    std::vector<std::uint8_t> privateKey;
    std::vector<std::uint8_t> publicKey;
};

struct JwtIssueRequest
{
    std::string subject;
    std::string jti;
    std::uint64_t authzVersion{};
};

struct JwtClaims
{
    std::string keyId;
    std::string issuer;
    std::string audience;
    std::string subject;
    std::string jti;
    std::int64_t issuedAt{};
    std::int64_t notBefore{};
    std::int64_t expiresAt{};
    std::uint64_t authzVersion{};
};

class JwtService final
{
public:
    JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config);

    [[nodiscard]] auto issue(const JwtIssueRequest &request, const JwtSigningKey &key)
        -> SZX::COMMON::Result<std::string>;

    [[nodiscard]] auto verify(std::string_view token, const JwtSigningKey &key)
        -> SZX::COMMON::Result<JwtClaims>;

private:
    SZX::CRYPTO_SERVICE::ICryptoService &crypto_;
    JwtConfig config_;
};
} // namespace SZX::ADMIN::AUTH
