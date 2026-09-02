#include "SzxKeyStore/KeyStore.h"
#include <utility>
#include "SzxCrypto/Hash.h"
#if defined(_WIN32)
    #include <windows.h>

    #include <bcrypt.h>

    #include <ncrypt.h>
    #pragma comment(lib, "ncrypt.lib")
#endif
namespace SZX::KEYSTORE
{
namespace
{
template <class T>
auto fail(KeyStoreError e, const char *m) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure((int)e, m);
}
auto validProvider(const std::string &provider) -> bool
{
    return provider.empty() || provider == "Microsoft Software Key Storage Provider";
}
} // namespace

auto createRsa3072(const std::string &name) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    return createRsa3072(name, KeyUsage::SignAndDecrypt);
}

auto createRsa3072(const std::string &name, KeyUsage usage) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    if (name.empty() || name.size() > 255)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "密钥名称无效。");
#if !defined(_WIN32)
    return fail<std::vector<std::uint8_t>>(KeyStoreError::Unsupported, "当前平台不支持 NCrypt。");
#else
    if (usage != KeyUsage::Sign && usage != KeyUsage::Decrypt && usage != KeyUsage::SignAndDecrypt)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "密钥用途无效。");
    NCRYPT_PROV_HANDLE provider{};
    if (NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开密钥存储 provider 失败。");
    std::wstring wide(name.begin(), name.end());
    NCRYPT_KEY_HANDLE key{};
    SECURITY_STATUS s = NCryptCreatePersistedKey(provider, &key, NCRYPT_RSA_ALGORITHM, wide.c_str(), 0, 0);
    if (s != ERROR_SUCCESS)
    {
        NCryptFreeObject(provider);
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "创建持久化密钥失败。");
    }
    DWORD bits = 3072;
    if (NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&bits), sizeof(bits), 0) != ERROR_SUCCESS)
    { NCryptFreeObject(key); NCryptFreeObject(provider); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "设置 RSA 密钥长度失败。"); }
    DWORD usageFlags = static_cast<DWORD>(usage) == 1 ? NCRYPT_ALLOW_SIGNING_FLAG :
        static_cast<DWORD>(usage) == 2                ? NCRYPT_ALLOW_DECRYPT_FLAG :
                                                        (NCRYPT_ALLOW_SIGNING_FLAG | NCRYPT_ALLOW_DECRYPT_FLAG);
    if (NCryptSetProperty(key, NCRYPT_KEY_USAGE_PROPERTY, reinterpret_cast<PBYTE>(&usageFlags), sizeof(usageFlags), 0) != ERROR_SUCCESS)
    { NCryptFreeObject(key); NCryptFreeObject(provider); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "设置密钥用途失败。"); }
    DWORD exportPolicy = 0;
    if (NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, reinterpret_cast<PBYTE>(&exportPolicy), sizeof(exportPolicy), 0) != ERROR_SUCCESS)
    { NCryptFreeObject(key); NCryptFreeObject(provider); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "设置私钥导出策略失败。"); }
    s = NCryptFinalizeKey(key, 0);
    if (s != ERROR_SUCCESS)
    {
        NCryptFreeObject(key);
        NCryptFreeObject(provider);
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "完成持久化密钥失败。");
    }
    DWORD size = 0;
    s          = NCryptExportKey(key, 0, BCRYPT_RSAPUBLIC_BLOB, nullptr, nullptr, 0, &size, 0);
    if (s != ERROR_SUCCESS)
    {
        NCryptFreeObject(key);
        NCryptFreeObject(provider);
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "导出公钥失败。");
    }
    std::vector<std::uint8_t> out(size);
    s = NCryptExportKey(key, 0, BCRYPT_RSAPUBLIC_BLOB, nullptr, out.data(), size, &size, 0);
    NCryptFreeObject(key);
    NCryptFreeObject(provider);
    if (s != ERROR_SUCCESS)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "导出公钥失败。");
    out.resize(size);
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out));
#endif
}

auto deleteKey(const KeyReference &reference) -> SZX::COMMON::Result<void>
{
    if (reference.name.empty() || !validProvider(reference.provider))
        return SZX::COMMON::Result<void>::Failure((int)KeyStoreError::InvalidArgument, "密钥引用无效。");
#if !defined(_WIN32)
    return SZX::COMMON::Result<void>::Failure((int)KeyStoreError::Unsupported, "当前平台不支持 NCrypt。");
#else
    NCRYPT_PROV_HANDLE p{};
    if (NCryptOpenStorageProvider(&p, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS)
        return SZX::COMMON::Result<void>::Failure((int)KeyStoreError::ProviderFailure, "打开密钥存储 provider 失败。");
    std::wstring n(reference.name.begin(), reference.name.end());
    NCRYPT_KEY_HANDLE k{};
    auto s = NCryptOpenKey(p, &k, n.c_str(), 0, 0);
    if (s == NTE_BAD_KEYSET)
    {
        NCryptFreeObject(p);
        return SZX::COMMON::Result<void>::Failure((int)KeyStoreError::NotFound, "持久化密钥不存在。");
    }
    if (s != ERROR_SUCCESS) { NCryptFreeObject(p); return SZX::COMMON::Result<void>::Failure((int)KeyStoreError::ProviderFailure, "打开持久化密钥失败。"); }
    s = NCryptDeleteKey(k, 0);
    NCryptFreeObject(p);
    return s == ERROR_SUCCESS ? SZX::COMMON::Result<void>::Success() :
                                SZX::COMMON::Result<void>::Failure((int)KeyStoreError::ProviderFailure, "删除持久化密钥失败。");
#endif
}

auto openRsaPublicKey(const KeyReference &reference) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    if (reference.name.empty() || !validProvider(reference.provider))
        return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "密钥引用无效。");
#if !defined(_WIN32)
    return fail<std::vector<std::uint8_t>>(KeyStoreError::Unsupported, "当前平台不支持 NCrypt。");
#else
    NCRYPT_PROV_HANDLE p{};
    if (NCryptOpenStorageProvider(&p, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开密钥存储 provider 失败。");
    std::wstring n(reference.name.begin(), reference.name.end());
    NCRYPT_KEY_HANDLE k{};
    auto s = NCryptOpenKey(p, &k, n.c_str(), 0, 0);
    if (s == NTE_BAD_KEYSET)
    {
        NCryptFreeObject(p);
        return fail<std::vector<std::uint8_t>>(KeyStoreError::NotFound, "持久化密钥不存在。");
    }
    if (s != ERROR_SUCCESS) { NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开持久化密钥失败。"); }
    DWORD size = 0;
    s          = NCryptExportKey(k, 0, BCRYPT_RSAPUBLIC_BLOB, nullptr, nullptr, 0, &size, 0);
    if (s != ERROR_SUCCESS)
    {
        NCryptFreeObject(k);
        NCryptFreeObject(p);
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "导出公钥失败。");
    }
    std::vector<std::uint8_t> out(size);
    s = NCryptExportKey(k, 0, BCRYPT_RSAPUBLIC_BLOB, nullptr, out.data(), size, &size, 0);
    NCryptFreeObject(k);
    NCryptFreeObject(p);
    if (s != ERROR_SUCCESS)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "导出公钥失败。");
    out.resize(size);
    return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out));
#endif
}
auto signPssSha256(const KeyReference &reference, std::span<const std::uint8_t> data) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    if (reference.name.empty() || !validProvider(reference.provider) || data.size() > 0xffffffffULL)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "签名参数无效。");
#if !defined(_WIN32)
    return fail<std::vector<std::uint8_t>>(KeyStoreError::Unsupported, "当前平台不支持 NCrypt。");
#else
    NCRYPT_PROV_HANDLE p{}; if (NCryptOpenStorageProvider(&p, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS) return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开密钥存储 provider 失败。");
    std::wstring n(reference.name.begin(), reference.name.end()); NCRYPT_KEY_HANDLE k{}; auto s = NCryptOpenKey(p, &k, n.c_str(), 0, 0);
    if (s == NTE_BAD_KEYSET) { NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::NotFound, "持久化密钥不存在。"); }
    if (s != ERROR_SUCCESS) { NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开持久化密钥失败。"); }
    DWORD usage = 0, size = sizeof(usage); s = NCryptGetProperty(k, NCRYPT_KEY_USAGE_PROPERTY, reinterpret_cast<PBYTE>(&usage), sizeof(usage), &size, 0);
    if (s != ERROR_SUCCESS || (usage & NCRYPT_ALLOW_SIGNING_FLAG) == 0) { NCryptFreeObject(k); NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "密钥不允许签名。"); }
    auto hash = SZX::CRYPTO::digest(SZX::CRYPTO::HashAlgorithm::Sha256, data); if (!hash.ok) { NCryptFreeObject(k); NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "计算签名摘要失败。"); }
    BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 32}; DWORD outSize = 0; s = NCryptSignHash(k, &padding, hash.value.data(), static_cast<DWORD>(hash.value.size()), nullptr, 0, &outSize, NCRYPT_PAD_PSS_FLAG);
    if (s != ERROR_SUCCESS) { NCryptFreeObject(k); NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "签名失败。"); }
    std::vector<std::uint8_t> out(outSize); s = NCryptSignHash(k, &padding, hash.value.data(), static_cast<DWORD>(hash.value.size()), out.data(), outSize, &outSize, NCRYPT_PAD_PSS_FLAG); NCryptFreeObject(k); NCryptFreeObject(p); if (s != ERROR_SUCCESS) return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "签名失败。"); out.resize(outSize); return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out));
#endif
}
auto decryptOaepSha256(const KeyReference &reference, std::span<const std::uint8_t> ciphertext) -> SZX::COMMON::Result<std::vector<std::uint8_t>>
{
    if (reference.name.empty() || !validProvider(reference.provider) || ciphertext.size() > 0xffffffffULL)
        return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "解密参数无效。");
#if !defined(_WIN32)
    return fail<std::vector<std::uint8_t>>(KeyStoreError::Unsupported, "当前平台不支持 NCrypt。");
#else
    NCRYPT_PROV_HANDLE p{}; if (NCryptOpenStorageProvider(&p, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS) return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开密钥存储 provider 失败。");
    std::wstring n(reference.name.begin(), reference.name.end()); NCRYPT_KEY_HANDLE k{}; auto s = NCryptOpenKey(p, &k, n.c_str(), 0, 0);
    if (s == NTE_BAD_KEYSET) { NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::NotFound, "持久化密钥不存在。"); }
    if (s != ERROR_SUCCESS) { NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "打开持久化密钥失败。"); }
    DWORD usage = 0, propSize = sizeof(usage); s = NCryptGetProperty(k, NCRYPT_KEY_USAGE_PROPERTY, reinterpret_cast<PBYTE>(&usage), sizeof(usage), &propSize, 0);
    if (s != ERROR_SUCCESS || (usage & NCRYPT_ALLOW_DECRYPT_FLAG) == 0) { NCryptFreeObject(k); NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::InvalidArgument, "密钥不允许解密。"); }
    BCRYPT_OAEP_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, nullptr, 0}; DWORD outSize = 0; s = NCryptDecrypt(k, const_cast<PBYTE>(ciphertext.data()), static_cast<DWORD>(ciphertext.size()), &padding, nullptr, 0, &outSize, NCRYPT_PAD_OAEP_FLAG);
    if (s != ERROR_SUCCESS) { NCryptFreeObject(k); NCryptFreeObject(p); return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "RSA-OAEP 解密失败。"); }
    std::vector<std::uint8_t> out(outSize); s = NCryptDecrypt(k, const_cast<PBYTE>(ciphertext.data()), static_cast<DWORD>(ciphertext.size()), &padding, out.data(), outSize, &outSize, NCRYPT_PAD_OAEP_FLAG); NCryptFreeObject(k); NCryptFreeObject(p); if (s != ERROR_SUCCESS) return fail<std::vector<std::uint8_t>>(KeyStoreError::ProviderFailure, "RSA-OAEP 解密失败。"); out.resize(outSize); return SZX::COMMON::Result<std::vector<std::uint8_t>>::Success(std::move(out));
#endif
}
} // namespace SZX::KEYSTORE
