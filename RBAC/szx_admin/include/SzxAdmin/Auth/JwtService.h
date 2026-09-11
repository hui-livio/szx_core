#pragma once

#include "SzxCryptoService/CryptoService.h"

#include <cstdint>
#include <map>
#include <shared_mutex>
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

    [[nodiscard]] static auto loadFromEnvironment() -> SZX::COMMON::Result<JwtConfig>;
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

struct JwtKeyRecord
{
    JwtSigningKey key;
    bool activeForSigning{};
    std::int64_t notBefore{};
    std::int64_t expiresAt{};
};

class JwtKeyStore final
{
public:
    [[nodiscard]] auto addKey(JwtKeyRecord record) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto activateKey(std::string_view keyId, std::int64_t now = 0) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto signingKey(std::int64_t now = 0) const -> SZX::COMMON::Result<JwtSigningKey>;
    [[nodiscard]] auto verificationKey(std::string_view keyId, std::int64_t now = 0) const
        -> SZX::COMMON::Result<JwtSigningKey>;

private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, JwtKeyRecord, std::less<>> keys_;
};

class JwtService final
{
public:
    JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config);
    JwtService(SZX::CRYPTO_SERVICE::ICryptoService &crypto, JwtConfig config, JwtKeyStore &keyStore);

    [[nodiscard]] auto issue(const JwtIssueRequest &request) -> SZX::COMMON::Result<std::string>;
    [[nodiscard]] auto verify(std::string_view token) -> SZX::COMMON::Result<JwtClaims>;

    [[nodiscard]] auto issue(const JwtIssueRequest &request, const JwtSigningKey &key)
        -> SZX::COMMON::Result<std::string>;

    [[nodiscard]] auto verify(std::string_view token, const JwtSigningKey &key)
        -> SZX::COMMON::Result<JwtClaims>;

private:
    SZX::CRYPTO_SERVICE::ICryptoService &crypto_;
    JwtConfig config_;
    JwtKeyStore *keyStore_{};
};
} // namespace SZX::ADMIN::AUTH
