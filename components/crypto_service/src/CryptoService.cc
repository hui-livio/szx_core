#include "SzxCryptoService/CryptoService.h"

namespace SZX::CRYPTO_SERVICE
{
auto CryptoService::capabilities() const -> CryptoCapabilities
{
    return {.aes256Gcm = true, .rsaOaepSha256 = true, .rsaPssSha256 = true, .pbkdf2Sha256 = true};
}

auto CryptoService::encrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data,
                            std::span<const std::uint8_t> aad) -> SZX::COMMON::Result<SZX::CRYPTO::AesGcmCiphertext>
{
    return SZX::CRYPTO::aes256GcmEncrypt(key, data, aad);
}

auto CryptoService::decrypt(std::span<const std::uint8_t> key, const SZX::CRYPTO::AesGcmCiphertext &cipher,
                            std::span<const std::uint8_t> aad) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return SZX::CRYPTO::aes256GcmDecrypt(key, cipher.nonce, cipher.ciphertext, cipher.tag, aad);
}

auto CryptoService::rsaEncrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return SZX::CRYPTO::rsaOaepSha256Encrypt(key, data);
}

auto CryptoService::rsaDecrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return SZX::CRYPTO::rsaOaepSha256Decrypt(key, data);
}

auto CryptoService::sign(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return SZX::CRYPTO::rsaPssSha256Sign(key, data);
}

auto CryptoService::verify(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data,
                           std::span<const std::uint8_t> sig) -> SZX::COMMON::Result<bool>
{
    return SZX::CRYPTO::rsaPssSha256Verify(key, data, sig);
}

auto CryptoService::derive(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, std::uint32_t iterations,
                           std::size_t length) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return SZX::CRYPTO::derivePbkdf2Sha256(password, salt, iterations, length);
}
namespace { template<class T> auto fakeUnsupported() -> SZX::COMMON::Result<T> { return SZX::COMMON::Result<T>::Failure(-1, "FakeCryptoService 未配置实现。"); } }
auto FakeCryptoService::encrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>) -> SZX::COMMON::Result<SZX::CRYPTO::AesGcmCiphertext> { ++encryptCalls; return fakeUnsupported<SZX::CRYPTO::AesGcmCiphertext>(); }
auto FakeCryptoService::decrypt(std::span<const std::uint8_t>, const SZX::CRYPTO::AesGcmCiphertext &, std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::vector<std::uint8_t>> { ++decryptCalls; return fakeUnsupported<std::vector<std::uint8_t>>(); }
auto FakeCryptoService::rsaEncrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::vector<std::uint8_t>> { return fakeUnsupported<std::vector<std::uint8_t>>(); }
auto FakeCryptoService::rsaDecrypt(std::span<const std::uint8_t>, std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::vector<std::uint8_t>> { return fakeUnsupported<std::vector<std::uint8_t>>(); }
auto FakeCryptoService::sign(std::span<const std::uint8_t>, std::span<const std::uint8_t>) -> SZX::COMMON::Result<std::vector<std::uint8_t>> { return fakeUnsupported<std::vector<std::uint8_t>>(); }
auto FakeCryptoService::verify(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::uint8_t>) -> SZX::COMMON::Result<bool> { return fakeUnsupported<bool>(); }
auto FakeCryptoService::derive(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::uint32_t, std::size_t) -> SZX::COMMON::Result<std::vector<std::uint8_t>> { return fakeUnsupported<std::vector<std::uint8_t>>(); }
} // namespace SZX::CRYPTO_SERVICE
