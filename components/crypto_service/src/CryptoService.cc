#include "SzxCryptoService/CryptoService.h"
#include <exception>
#include "SzxCrypto/Crypto.h"

namespace SZX::CRYPTO_SERVICE
{
namespace
{
auto mapCryptoError(int error) -> int
{
    switch (error)
    {
    case static_cast<int>(SZX::CRYPTO::Argon2Error::InvalidArgument):
    case static_cast<int>(SZX::CRYPTO::RsaError::InvalidArgument):
        return static_cast<int>(CryptoServiceError::InvalidArgument);
    case static_cast<int>(SZX::CRYPTO::Argon2Error::MalformedHash):
        return static_cast<int>(CryptoServiceError::MalformedHash);
    case static_cast<int>(SZX::CRYPTO::Argon2Error::AuthenticationFailed):
    case static_cast<int>(SZX::CRYPTO::RsaError::AuthenticationFailed):
        return static_cast<int>(CryptoServiceError::AuthenticationFailed);
    case static_cast<int>(SZX::CRYPTO::Argon2Error::Unsupported):
    case static_cast<int>(SZX::CRYPTO::RsaError::Unsupported):
        return static_cast<int>(CryptoServiceError::Unsupported);
    default:
        return static_cast<int>(CryptoServiceError::ProviderFailure);
    }
}

template <typename T>
auto mapCryptoResult(const SZX::COMMON::Result<T> &result) -> SZX::COMMON::Result<T>
{
    return result.ok ? SZX::COMMON::Result<T>::Success(result.value) :
                       SZX::COMMON::Result<T>::Failure(mapCryptoError(result.error), result.message);
}

template <typename Function>
auto invokeBoundary(Function &&function) -> decltype(function())
{
    using ResultType = decltype(function());
    try
    {
        return function();
    } catch (const std::exception &)
    {
        return ResultType::Failure(static_cast<int>(CryptoServiceError::InternalFailure), "加密服务内部异常。");
    } catch (...)
    {
        return ResultType::Failure(static_cast<int>(CryptoServiceError::InternalFailure), "加密服务内部异常。");
    }
}
} // namespace
auto CryptoService::capabilities() const -> CryptoCapabilities
{
    return {.aes256Gcm     = true,
            .rsaOaepSha256 = true,
            .rsaPssSha256  = true,
            .pbkdf2Sha256  = true,
            .argon2id      = true,
            .ps256Jwt      = true,
            .publicKeyId   = true};
}

auto CryptoService::encrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data,
                            std::span<const std::uint8_t> aad) -> SZX::COMMON::Result<CryptoCiphertext>
{
    return invokeBoundary(
        [&]() -> SZX::COMMON::Result<CryptoCiphertext>
        {
            auto r = SZX::CRYPTO::aes256GcmEncrypt(key, data, aad);
            if (!r.ok)
                return SZX::COMMON::Result<CryptoCiphertext>::Failure(r.error, r.message);
            return SZX::COMMON::Result<CryptoCiphertext>::Success(
                {std::move(r.value.nonce), std::move(r.value.ciphertext), std::move(r.value.tag)});
        });
}

auto CryptoService::decrypt(std::span<const std::uint8_t> key, const CryptoCiphertext &cipher, std::span<const std::uint8_t> aad)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return SZX::CRYPTO::aes256GcmDecrypt(key, cipher.nonce, cipher.ciphertext, cipher.tag, aad); });
}

auto CryptoService::rsaEncrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return SZX::CRYPTO::rsaOaepSha256Encrypt(key, data); });
}

auto CryptoService::rsaDecrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return SZX::CRYPTO::rsaOaepSha256Decrypt(key, data); });
}

auto CryptoService::sign(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return SZX::CRYPTO::rsaPssSha256Sign(key, data); });
}

auto CryptoService::verify(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data,
                           std::span<const std::uint8_t> sig) -> SZX::COMMON::Result<bool>
{
    return invokeBoundary([&] { return SZX::CRYPTO::rsaPssSha256Verify(key, data, sig); });
}

auto CryptoService::derive(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, std::uint32_t iterations,
                           std::size_t length) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return invokeBoundary([&] { return SZX::CRYPTO::derivePbkdf2Sha256(password, salt, iterations, length); });
}

auto CryptoService::hashPassword(std::span<const std::uint8_t> password, const Argon2idParameters &parameters)
    -> SZX::COMMON::Result<std::string>
{
    return invokeBoundary(
        [&]
        {
            const SZX::CRYPTO::Argon2idParameters cryptoParameters{
                parameters.memoryCostKiB, parameters.iterations, parameters.lanes, parameters.saltLength, parameters.hashLength};
            return mapCryptoResult(SZX::CRYPTO::hashArgon2id(password, cryptoParameters));
        });
}

auto CryptoService::verifyPassword(std::span<const std::uint8_t> password, std::string_view encodedHash)
    -> SZX::COMMON::Result<bool>
{
    return invokeBoundary(
        [&]
        {
            const auto result = SZX::CRYPTO::verifyArgon2id(password, encodedHash);
            return result.ok ? SZX::COMMON::Result<bool>::Success(result.value) :
                               SZX::COMMON::Result<bool>::Failure(mapCryptoError(result.error), result.message);
        });
}

auto CryptoService::publicKeyId(std::span<const std::uint8_t> publicKey) -> SZX::COMMON::Result<std::string>
{
    return invokeBoundary(
        [&]
        {
            const auto result = SZX::CRYPTO::rsaPublicKeyId(publicKey);
            return result.ok ? SZX::COMMON::Result<std::string>::Success(std::move(result.value)) :
                               SZX::COMMON::Result<std::string>::Failure(mapCryptoError(result.error), result.message);
        });
}

auto CryptoService::signPs256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return sign(key, data);
}

auto CryptoService::verifyPs256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data,
                                std::span<const std::uint8_t> signature) -> SZX::COMMON::Result<bool>
{
    return verify(key, data, signature);
}
namespace
{
template <class T>
auto fakeUnsupported() -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(CryptoServiceError::Unsupported), "FakeCryptoService 未配置实现。");
}
} // namespace
auto FakeCryptoService::encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<CryptoCiphertext>
{
    ++encryptCalls;
    return fakeUnsupported<CryptoCiphertext>();
}
auto FakeCryptoService::decrypt(std::span<const std::uint8_t>, const CryptoCiphertext &, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    ++decryptCalls;
    return fakeUnsupported<std::vector<std::uint8_t>>();
}
auto FakeCryptoService::rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return fakeUnsupported<std::vector<std::uint8_t>>();
}
auto FakeCryptoService::rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return fakeUnsupported<std::vector<std::uint8_t>>();
}
auto FakeCryptoService::sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return fakeUnsupported<std::vector<std::uint8_t>>();
}
auto FakeCryptoService::verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<bool>
{
    return fakeUnsupported<bool>();
}
auto FakeCryptoService::derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return fakeUnsupported<std::vector<std::uint8_t>>();
}
auto FakeCryptoService::hashPassword(std::span<const std::uint8_t>, const Argon2idParameters &)
    -> SZX::COMMON::Result<std::string>
{
    return fakeUnsupported<std::string>();
}
auto FakeCryptoService::verifyPassword(std::span<const std::uint8_t>, std::string_view) -> SZX::COMMON::Result<bool>
{
    return fakeUnsupported<bool>();
}
auto FakeCryptoService::publicKeyId(std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::string>
{
    return fakeUnsupported<std::string>();
}
auto FakeCryptoService::signPs256(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return fakeUnsupported<std::vector<std::uint8_t>>();
}
auto FakeCryptoService::verifyPs256(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    -> SZX::COMMON::Result<bool>
{
    return fakeUnsupported<bool>();
}
} // namespace SZX::CRYPTO_SERVICE
