#include "OpenSslInternal.hpp"

#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include "SzxCrypto/Argon2.h"
#include "SzxCrypto/Base64.h"
#include "SzxCrypto/Random.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace SZX::COMMON;

namespace SZX::CRYPTO
{
using namespace internal;

namespace
{
constexpr std::uint32_t kArgon2Version    = 0x13;
constexpr std::uint32_t kMaxMemoryCostKiB = 1024U * 1024U;
constexpr std::uint32_t kMaxIterations    = 100U;
constexpr std::uint32_t kMaxLanes         = 32U;
constexpr std::size_t kMaxPasswordLength  = 1024U * 1024U;
constexpr std::size_t kMaxSaltLength      = 1024U;
constexpr std::size_t kMaxHashLength      = 1024U;

auto invalidArgument(const char *message) -> Result<std::string>
{
    return Result<std::string>::Failure(static_cast<int>(Argon2Error::InvalidArgument), message);
}

auto malformedHash(const char *message) -> Result<bool>
{
    return Result<bool>::Failure(static_cast<int>(Argon2Error::MalformedHash), message);
}

auto parseUnsigned(std::string_view text, std::uint32_t &value) -> bool
{
    if (text.empty())
        return false;
    const auto *first = text.data();
    const auto *last  = first + text.size();
    const auto parsed = std::from_chars(first, last, value);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

auto isUnpaddedBase64(std::string_view text) -> bool
{
    if (text.empty())
        return false;
    for (const auto character : text)
    {
        const bool valid = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9') || character == '+' || character == '/';
        if (!valid)
            return false;
    }
    return true;
}

struct ParsedHash
{
    Argon2idParameters parameters;
    std::vector<std::uint8_t> salt;
    std::vector<std::uint8_t> digest;
};

auto parseHash(std::string_view encoded, ParsedHash &parsed) -> bool
{
    constexpr std::string_view prefix = "$argon2id$v=19$m=";
    if (!encoded.starts_with(prefix))
        return false;
    const auto paramsEnd = encoded.find('$', prefix.size());
    if (paramsEnd == std::string_view::npos)
        return false;
    const auto parametersText = encoded.substr(prefix.size(), paramsEnd - prefix.size());
    const auto firstComma     = parametersText.find(",t=");
    const auto secondComma    = parametersText.find(",p=");
    if (firstComma == std::string_view::npos || secondComma == std::string_view::npos || firstComma >= secondComma)
        return false;
    std::uint32_t memory = 0, iterations = 0, lanes = 0;
    if (!parseUnsigned(parametersText.substr(0, firstComma), memory) ||
        !parseUnsigned(parametersText.substr(firstComma + 3, secondComma - firstComma - 3), iterations) ||
        !parseUnsigned(parametersText.substr(secondComma + 3), lanes))
        return false;
    parsed.parameters.memoryCostKiB = memory;
    parsed.parameters.iterations    = iterations;
    parsed.parameters.lanes         = lanes;

    const auto saltEnd = encoded.find('$', paramsEnd + 1);
    if (saltEnd == std::string_view::npos || saltEnd + 1 >= encoded.size())
        return false;
    const auto saltText   = encoded.substr(paramsEnd + 1, saltEnd - paramsEnd - 1);
    const auto digestText = encoded.substr(saltEnd + 1);
    auto salt             = decodeBase64(saltText);
    auto digest           = decodeBase64(digestText);
    if (!isUnpaddedBase64(saltText) || !isUnpaddedBase64(digestText) || !salt.ok || !digest.ok || salt.value.empty() ||
        digest.value.empty())
        return false;
    parsed.salt                  = std::move(salt.value);
    parsed.digest                = std::move(digest.value);
    parsed.parameters.saltLength = parsed.salt.size();
    parsed.parameters.hashLength = parsed.digest.size();
    return true;
}

auto validParameters(std::span<const std::uint8_t> password, const Argon2idParameters &parameters) -> bool
{
    return !password.empty() && password.size() <= kMaxPasswordLength && parameters.memoryCostKiB >= 8U * parameters.lanes &&
        parameters.memoryCostKiB <= kMaxMemoryCostKiB && parameters.iterations >= 1U && parameters.iterations <= kMaxIterations &&
        parameters.lanes >= 1U && parameters.lanes <= kMaxLanes && parameters.saltLength >= 16U &&
        parameters.saltLength <= kMaxSaltLength && parameters.hashLength >= 16U && parameters.hashLength <= kMaxHashLength;
}

auto derive(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, const Argon2idParameters &parameters)
    -> Result<std::vector<std::uint8_t>>
{
    if (!validParameters(password, parameters) || salt.size() != parameters.saltLength)
        return Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(Argon2Error::InvalidArgument),
                                                          "Argon2id parameters invalid.");
    Kdf kdf(EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr));
    if (!kdf)
        return Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(Argon2Error::Unsupported),
                                                          "Argon2id provider is unavailable.");
    KdfCtx context(EVP_KDF_CTX_new(kdf.get()));
    if (!context)
        return Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(Argon2Error::ProviderFailure),
                                                          "Argon2id context creation failed.");
    auto memory         = parameters.memoryCostKiB;
    auto iterations     = parameters.iterations;
    auto lanes          = parameters.lanes;
    auto version        = kArgon2Version;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, const_cast<std::uint8_t *>(password.data()), password.size()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<std::uint8_t *>(salt.data()), salt.size()),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memory),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_VERSION, &version),
        OSSL_PARAM_construct_end(),
    };
    std::vector<std::uint8_t> output(parameters.hashLength);
    if (EVP_KDF_derive(context.get(), output.data(), output.size(), params) != 1)
        return Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(Argon2Error::ProviderFailure),
                                                          "Argon2id derivation failed.");
    return Result<std::vector<std::uint8_t>>::Success(std::move(output));
}

auto hashImpl(std::span<const std::uint8_t> password, const Argon2idParameters &parameters) -> Result<std::string>
{
    if (!validParameters(password, parameters))
        return invalidArgument("Argon2id parameters invalid.");
    auto salt = randomBytes(parameters.saltLength);
    if (!salt.ok)
        return Result<std::string>::Failure(static_cast<int>(Argon2Error::ProviderFailure), "Argon2id salt generation failed.");
    auto digest = derive(password, salt.value, parameters);
    if (!digest.ok)
        return Result<std::string>::Failure(digest.error, digest.message);
    auto encodedSalt   = encodeBase64(salt.value, Base64Alphabet::Standard, false);
    auto encodedDigest = encodeBase64(digest.value, Base64Alphabet::Standard, false);
    if (!encodedSalt.ok || !encodedDigest.ok)
        return Result<std::string>::Failure(static_cast<int>(Argon2Error::ProviderFailure), "Argon2id encoding failed.");
    return Result<std::string>::Success("$argon2id$v=19$m=" + std::to_string(parameters.memoryCostKiB) +
                                        ",t=" + std::to_string(parameters.iterations) + ",p=" + std::to_string(parameters.lanes) +
                                        "$" + encodedSalt.value + "$" + encodedDigest.value);
}

auto verifyImpl(std::span<const std::uint8_t> password, std::string_view encodedHash) -> Result<bool>
{
    if (password.empty() || password.size() > kMaxPasswordLength)
        return Result<bool>::Failure(static_cast<int>(Argon2Error::InvalidArgument), "Argon2id password invalid.");
    ParsedHash parsed;
    if (!parseHash(encodedHash, parsed) || !validParameters(password, parsed.parameters))
        return malformedHash("Argon2id hash format or parameters invalid.");
    auto digest = derive(password, parsed.salt, parsed.parameters);
    if (!digest.ok)
        return Result<bool>::Failure(digest.error, digest.message);
    return Result<bool>::Success(SZX::CRYPTO::constantTimeEqual(digest.value, parsed.digest));
}
} // namespace

auto hashArgon2id(std::span<const std::uint8_t> password, const Argon2idParameters &parameters) -> Result<std::string>
{
    return invokeBoundary([&] { return hashImpl(password, parameters); });
}

auto verifyArgon2id(std::span<const std::uint8_t> password, std::string_view encodedHash) -> Result<bool>
{
    return invokeBoundary([&] { return verifyImpl(password, encodedHash); });
}
} // namespace SZX::CRYPTO
