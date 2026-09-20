#include "PasswordHash.h"
#include "SzxAdmin/Infra/ErrorCode.h"

#include <algorithm>

namespace SZX::ADMIN::AUTH
{
auto validClientPasswordHash(std::string_view hash) noexcept -> bool
{
    return hash.size() == 64 &&
        std::all_of(hash.begin(), hash.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

auto storeClientPasswordHash(SZX::CRYPTO_SERVICE::ICryptoService &crypto, std::string_view hash)
    -> SZX::COMMON::Result<std::string>
{
    using SZX::COMMON::Result;
    try
    {
        if (!validClientPasswordHash(hash))
            return Result<std::string>::Failure(static_cast<int>(INFRA::ErrorCode::ValidationError), {});
        const auto stored = crypto.hashPassword({reinterpret_cast<const std::uint8_t *>(hash.data()), hash.size()});
        if (!stored.ok)
            return Result<std::string>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
        return Result<std::string>::Success(std::string{kPasswordHashPrefix} + stored.value);
    } catch (...)
    {
        return Result<std::string>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}

auto verifyClientPasswordHash(SZX::CRYPTO_SERVICE::ICryptoService &crypto, std::string_view hash, std::string_view stored)
    -> SZX::COMMON::Result<bool>
{
    using SZX::COMMON::Result;
    try
    {
        if (!validClientPasswordHash(hash))
            return Result<bool>::Failure(static_cast<int>(INFRA::ErrorCode::ValidationError), {});
        if (!stored.starts_with(kPasswordHashPrefix))
            return Result<bool>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
        const auto result = crypto.verifyPassword({reinterpret_cast<const std::uint8_t *>(hash.data()), hash.size()},
                                                  stored.substr(kPasswordHashPrefix.size()));
        if (!result.ok)
            return Result<bool>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
        return Result<bool>::Success(result.value);
    } catch (...)
    {
        return Result<bool>::Failure(static_cast<int>(INFRA::ErrorCode::InternalFailure), {});
    }
}
} // namespace SZX::ADMIN::AUTH
