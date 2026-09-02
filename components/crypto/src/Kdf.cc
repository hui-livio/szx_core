#include "SzxCrypto/Kdf.h"

#include <utility>

#if defined(_WIN32)
    #include <bcrypt.h>
    #include <windows.h>
    #pragma comment(lib, "bcrypt.lib")
#endif

namespace SZX::CRYPTO
{
auto derivePbkdf2Sha256(std::span<const std::uint8_t> password, std::span<const std::uint8_t> salt, std::uint32_t iterations,
                        std::size_t keyLength) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    constexpr std::size_t kMaxKeyLength = 1024 * 1024;
    if (salt.empty() || iterations == 0 || keyLength == 0 || keyLength > kMaxKeyLength ||
        password.size() > static_cast<std::size_t>(0xffffffffU) || salt.size() > static_cast<std::size_t>(0xffffffffU))
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(KdfError::InvalidArgument),
                                                                       "PBKDF2 参数无效。");
#if !defined(_WIN32)
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(KdfError::Unsupported),
                                                                   "当前平台不支持 CNG PBKDF2。");
#else
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(KdfError::ProviderFailure),
                                                                       "打开 HMAC-SHA256 provider 失败。");
    std::vector<std::uint8_t> output(keyLength);
    NTSTATUS status = BCryptDeriveKeyPBKDF2(algorithm, const_cast<PUCHAR>(password.data()), static_cast<ULONG>(password.size()),
                                            const_cast<PUCHAR>(salt.data()), static_cast<ULONG>(salt.size()), iterations,
                                            output.data(), static_cast<ULONG>(output.size()), 0);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status != 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(KdfError::ProviderFailure),
                                                                       "PBKDF2 派生失败。");
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(output));
#endif
}
} // namespace SZX::CRYPTO
