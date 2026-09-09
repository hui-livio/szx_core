#include "FileHelper.hpp"
#include "SzxCrypto/Rsa.h"
#include "SzxLicense/Issuer.h"
#include "SzxLicense/License.h"
#include "TimeHelper.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr std::uintmax_t kMaxInputFile = 2U * 1024U * 1024U;
auto readText(const std::string &path) -> std::optional<std::string>
{
    const auto size = SZX::COMMON::FileHelper::Size(path);
    if (!size || *size > kMaxInputFile)
        return std::nullopt;
    return SZX::COMMON::FileHelper::ReadText(path);
}
auto readBytes(const std::string &path) -> std::optional<std::vector<std::uint8_t>>
{
    const auto size = SZX::COMMON::FileHelper::Size(path);
    if (!size || *size > kMaxInputFile)
        return std::nullopt;
    return SZX::COMMON::FileHelper::ReadBytes(path);
}
auto readRsaKey(const std::string &path, bool privateKey) -> std::optional<std::vector<std::uint8_t>>
{
    auto bytes = readBytes(path);
    if (!bytes)
        return std::nullopt;
    auto converted = privateKey ? SZX::CRYPTO::rsaPrivateKeyFromEncoded(*bytes) : SZX::CRYPTO::rsaPublicKeyFromEncoded(*bytes);
    return converted.ok ? std::optional<std::vector<std::uint8_t>>(std::move(converted.value)) : std::nullopt;
}
auto writeBytes(const std::string &path, const std::vector<std::uint8_t> &bytes, bool overwrite = false) -> bool
{
    if (!overwrite && SZX::COMMON::FileHelper::Exists(path))
        return false;
    return SZX::COMMON::FileHelper::WriteBytes(path, bytes);
}
void usage()
{
    std::cerr << "usage: szx_license_issuer keygen --private-key <pem> --public-key <pem> [--force]\n"
                 "       szx_license_issuer request --product <id> --version <ver> --public-key <der|pem> --output <req>\n"
                 "       szx_license_issuer issue --request <req> --private-key <der|pem> --public-key <der|pem> --output <lic>"
                 " [--license-id <id>] [--issuer <name>] [--days <n>]\n"
                 "       szx_license_issuer key-info --public-key <der|pem>\n"
                 "       szx_license_issuer verify --input <lic> --public-key <der|pem> [--product <id>]\n";
}
} // namespace

int runMain(int argc, char **argv)
{
    if (argc < 2)
    {
        usage();
        return 2;
    }
    const std::string command = argv[1];
    auto arg                  = [&](std::string_view name) -> std::optional<std::string>
    {
        for (int i = 2; i + 1 < argc; ++i)
            if (argv[i] == name)
                return argv[i + 1];
        return std::nullopt;
    };
    auto hasFlag = [&](std::string_view name)
    {
        for (int i = 2; i < argc; ++i)
            if (argv[i] == name)
                return true;
        return false;
    };
    if (command == "keygen")
    {
        auto privateKey = arg("--private-key"), publicKey = arg("--public-key");
        if (!privateKey || !publicKey)
        {
            usage();
            return 2;
        }
        const bool force = hasFlag("--force");
        if (!force && (std::filesystem::exists(*privateKey) || std::filesystem::exists(*publicKey)))
        {
            std::cerr << "key file exists (use --force to overwrite)\n";
            return 1;
        }
        auto keys             = SZX::CRYPTO::generateRsa3072KeyPair();
        const auto privatePem = keys.ok ? SZX::CRYPTO::rsaPrivateKeyToPem(keys.value.privateKey) :
                                          SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1711, "key generation failed");
        const auto publicPem  = keys.ok ? SZX::CRYPTO::rsaPublicKeyToPem(keys.value.publicKey) :
                                          SZX::COMMON::Result<std::vector<std::uint8_t>>::Failure(1711, "key generation failed");
        if (!keys.ok || !privatePem.ok || !publicPem.ok || !writeBytes(*privateKey, privatePem.value, force) ||
            !writeBytes(*publicKey, publicPem.value, force))
        {
            std::cerr << (keys.ok ? "failed to write key files (use --force to overwrite)" : keys.message) << '\n';
            return 1;
        }
        auto id = SZX::CRYPTO::rsaPublicKeyId(keys.value.publicKey);
        if (id.ok)
            std::cout << "issuer_key_id=" << id.value << '\n';
        return 0;
    }
    if (command == "request")
    {
        auto product = arg("--product"), version = arg("--version"), publicKey = arg("--public-key"), output = arg("--output");
        if (!product || !version || !publicKey || !output)
        {
            usage();
            return 2;
        }
        auto publicBytes = readRsaKey(*publicKey, false);
        if (!publicBytes)
        {
            std::cerr << "failed to read public key\n";
            return 1;
        }
        auto result = SZX::LICENSE::createRequestFile({.productId = *product, .appVersion = *version}, *output, *publicBytes);
        if (!result.ok)
        {
            std::cerr << result.message << '\n';
            return 1;
        }
        return 0;
    }
    if (command == "key-info")
    {
        auto publicKey = arg("--public-key");
        if (!publicKey)
        {
            usage();
            return 2;
        }
        auto publicBytes = readRsaKey(*publicKey, false);
        if (!publicBytes)
        {
            std::cerr << "failed to read public key\n";
            return 1;
        }
        auto id = SZX::CRYPTO::rsaPublicKeyId(*publicBytes);
        if (!id.ok)
        {
            std::cerr << id.message << '\n';
            return 1;
        }
        std::cout << "issuer_key_id=" << id.value << '\n';
        return 0;
    }
    if (command == "issue")
    {
        auto request = arg("--request"), privateKey = arg("--private-key"), publicKey = arg("--public-key"),
             output = arg("--output");
        if (!request || !privateKey || !publicKey || !output)
        {
            usage();
            return 2;
        }
        auto privateBytes = readRsaKey(*privateKey, true);
        auto publicBytes  = readRsaKey(*publicKey, false);
        if (!privateBytes || !publicBytes)
        {
            std::cerr << "failed to read input\n";
            return 1;
        }
        SZX::LICENSE::IssueOptions options;
        options.licenseId =
            arg("--license-id").value_or("license-" + std::to_string(SZX::COMMON::TimeHelper::NowTimestamp() / 1000));
        options.issuer = arg("--issuer").value_or("szx-license-issuer");
        if (auto days = arg("--days"))
        {
            const auto count = std::stoll(*days);
            if (count <= 0 || count > std::numeric_limits<std::int64_t>::max() / 86400)
            {
                std::cerr << "--days is out of range\n";
                return 2;
            }
            options.expiresAtUtc = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + count * 86400;
        }
        auto result = SZX::LICENSE::issueFile(*request, options, *privateBytes, *publicBytes);
        if (!result.ok || !writeBytes(*output, result.value))
        {
            std::cerr << (result.ok ? "failed to write license" : result.message) << '\n';
            return 1;
        }
        return 0;
    }
    if (command == "verify")
    {
        auto input = arg("--input"), publicKey = arg("--public-key");
        if (!input || !publicKey)
        {
            usage();
            return 2;
        }
        auto key = readRsaKey(*publicKey, false);
        if (!key)
        {
            std::cerr << "failed to read input\n";
            return 1;
        }
        auto result = SZX::LICENSE::verifyFile(*input, *key, arg("--product").value_or(""));
        if (!result.ok)
        {
            std::cerr << result.message << '\n';
            return 1;
        }
        std::cout << result.value.licenseId << ' ' << result.value.productId << '\n';
        return 0;
    }
    usage();
    return 2;
}

int main(int argc, char **argv)
{
    try
    {
        return runMain(argc, argv);
    } catch (const std::exception &error)
    {
        std::cerr << "issuer failed: " << error.what() << '\n';
        return 1;
    } catch (...)
    {
        std::cerr << "issuer failed: unknown error\n";
        return 1;
    }
}
