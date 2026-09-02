#include "SzxCrypto/Aead.h"

#include "SzxCrypto/Random.h"

#include <utility>

#if defined(_WIN32)
    #include <windows.h>

    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#endif

namespace SZX::CRYPTO
{
namespace
{
constexpr std::size_t kKeySize   = 32;
constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kTagSize   = 16;
constexpr std::size_t kMaxCngLength = 0xffffffffULL;

#if defined(_WIN32)
struct AlgorithmHandle
{
    BCRYPT_ALG_HANDLE value{};
    ~AlgorithmHandle()
    {
        if (value)
            BCryptCloseAlgorithmProvider(value, 0);
    }
};
struct KeyHandle
{
    BCRYPT_KEY_HANDLE value{};
    ~KeyHandle()
    {
        if (value)
            BCryptDestroyKey(value);
    }
};
#endif

template <typename T>
auto invalid(const char *message) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(AeadError::InvalidArgument), message);
}
} // namespace

auto aes256GcmEncrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> plaintext,
                      std::span<const std::uint8_t> aad) -> SZX::COMMON::Result<AesGcmCiphertext>
{
    auto nonce = randomBytes(kNonceSize);
    if (!nonce.ok)
        return SZX::COMMON::Result<AesGcmCiphertext>::Failure(nonce.error, nonce.message);
    return aes256GcmEncryptWithNonce(key, nonce.value, plaintext, aad);
}

auto aes256GcmEncryptWithNonce(std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
                               std::span<const std::uint8_t> plaintext, std::span<const std::uint8_t> aad)
    -> SZX::COMMON::Result<AesGcmCiphertext>
{
    if (key.size() != kKeySize || nonce.size() != kNonceSize || plaintext.size() > kMaxCngLength || aad.size() > kMaxCngLength)
        return invalid<AesGcmCiphertext>("AES-256-GCM 密钥必须为 32 字节，nonce 必须为 12 字节。");
#if !defined(_WIN32)
    return SZX::COMMON::Result<AesGcmCiphertext>::Failure(static_cast<int>(AeadError::Unsupported),
                                                          "当前平台不支持 CNG AES-GCM。");
#else
    AlgorithmHandle algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0 ||
        BCryptSetProperty(algorithm.value, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM),
                          0) != 0)
        return SZX::COMMON::Result<AesGcmCiphertext>::Failure(static_cast<int>(AeadError::ProviderFailure),
                                                              "打开 AES-GCM provider 失败。");
    KeyHandle keyHandle;
    if (BCryptGenerateSymmetricKey(algorithm.value, &keyHandle.value, nullptr, 0,
                                   reinterpret_cast<PUCHAR>(const_cast<std::uint8_t *>(key.data())),
                                   static_cast<ULONG>(key.size()), 0) != 0)
        return SZX::COMMON::Result<AesGcmCiphertext>::Failure(static_cast<int>(AeadError::ProviderFailure),
                                                              "创建 AES 密钥失败。");
    AesGcmCiphertext output{std::vector<std::uint8_t>(nonce.begin(), nonce.end()), {}, std::vector<std::uint8_t>(kTagSize)};
    output.ciphertext.resize(plaintext.size());
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce    = const_cast<PUCHAR>(nonce.data());
    auth.cbNonce    = static_cast<ULONG>(nonce.size());
    auth.pbTag      = output.tag.data();
    auth.cbTag      = static_cast<ULONG>(output.tag.size());
    auth.pbAuthData = const_cast<PUCHAR>(aad.data());
    auth.cbAuthData = static_cast<ULONG>(aad.size());
    ULONG written   = 0;
    // CNG 对零长度输入/输出仍要求提供非空缓冲区；否则可能跳过 GCM
    // 认证流程，导致空明文使用错误密钥时被误判为成功。
    std::uint8_t dummy = 0;
    auto *input       = plaintext.empty() ? &dummy : const_cast<std::uint8_t *>(plaintext.data());
    auto *ciphertext  = output.ciphertext.empty() ? &dummy : output.ciphertext.data();
    if (BCryptEncrypt(keyHandle.value, input, static_cast<ULONG>(plaintext.size()), &auth, nullptr,
                      0, ciphertext, static_cast<ULONG>(output.ciphertext.size()), &written, 0) != 0)
        return SZX::COMMON::Result<AesGcmCiphertext>::Failure(static_cast<int>(AeadError::ProviderFailure), "AES-GCM 加密失败。");
    output.ciphertext.resize(written);
    return SZX::COMMON::Result<AesGcmCiphertext>::Success(std::move(output));
#endif
}

auto aes256GcmDecrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
                      std::span<const std::uint8_t> ciphertext, std::span<const std::uint8_t> tag,
                      std::span<const std::uint8_t> aad) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    if (key.size() != kKeySize || nonce.size() != kNonceSize || tag.size() != kTagSize || ciphertext.size() > kMaxCngLength || aad.size() > kMaxCngLength)
        return invalid<std::vector<std::uint8_t>>("AES-256-GCM 参数长度无效。");
#if !defined(_WIN32)
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(AeadError::Unsupported),
                                                                   "当前平台不支持 CNG AES-GCM。");
#else
    AlgorithmHandle algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0 ||
        BCryptSetProperty(algorithm.value, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM),
                          0) != 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(AeadError::ProviderFailure),
                                                                       "打开 AES-GCM provider 失败。");
    KeyHandle keyHandle;
    if (BCryptGenerateSymmetricKey(algorithm.value, &keyHandle.value, nullptr, 0,
                                   reinterpret_cast<PUCHAR>(const_cast<std::uint8_t *>(key.data())),
                                   static_cast<ULONG>(key.size()), 0) != 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(AeadError::ProviderFailure),
                                                                       "创建 AES 密钥失败。");
    std::vector<std::uint8_t> mutableTag(tag.begin(), tag.end());
    std::vector<std::uint8_t> output(ciphertext.size());
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce    = const_cast<PUCHAR>(nonce.data());
    auth.cbNonce    = static_cast<ULONG>(nonce.size());
    auth.pbTag      = mutableTag.data();
    auth.cbTag      = static_cast<ULONG>(mutableTag.size());
    auth.pbAuthData = const_cast<PUCHAR>(aad.data());
    auth.cbAuthData = static_cast<ULONG>(aad.size());
    ULONG written   = 0;
    std::uint8_t dummy = 0;
    auto *input       = ciphertext.empty() ? &dummy : const_cast<std::uint8_t *>(ciphertext.data());
    auto *plaintext   = output.empty() ? &dummy : output.data();
    NTSTATUS status = BCryptDecrypt(keyHandle.value, input, static_cast<ULONG>(ciphertext.size()),
                                     &auth, nullptr, 0, plaintext, static_cast<ULONG>(output.size()), &written, 0);
    if (status != 0)
        return SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(static_cast<int>(AeadError::AuthenticationFailed),
                                                                       "AES-GCM 认证失败。");
    output.resize(written);
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(output));
#endif
}
} // namespace SZX::CRYPTO
