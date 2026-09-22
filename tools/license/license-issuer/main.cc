#include "Command.hpp"
#include "FileHelper.hpp"
#include "SzxCrypto/Rsa.h"
#include "SzxLicense/Issuer.h"
#include "SzxLicense/License.h"
#include "TimeHelper.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr std::uintmax_t kMaxInputFile = 2U * 1024U * 1024U;

auto readBytes(const std::string &path) -> std::optional<std::vector<std::uint8_t>>
{
    return SZX::COMMON::FileHelper::ReadBytes(path, kMaxInputFile);
}

auto readRsaKey(const std::string &path, bool privateKey) -> std::optional<std::vector<std::uint8_t>>
{
    auto bytes = readBytes(path);
    if (!bytes)
        return std::nullopt;

    auto converted = privateKey ? SZX::CRYPTO::rsaPrivateKeyFromEncoded(*bytes) : SZX::CRYPTO::rsaPublicKeyFromEncoded(*bytes);
    if (!converted.ok)
        return std::nullopt;
    return std::optional<std::vector<std::uint8_t>>(std::move(converted.value));
}

auto writeBytes(const std::string &path, const std::vector<std::uint8_t> &bytes, bool overwrite = false) -> bool
{
    if (!overwrite && SZX::COMMON::FileHelper::Exists(path))
        return false;
    return SZX::COMMON::FileHelper::WriteBytes(path, bytes);
}

auto runKeygen(const std::string &privatePath, const std::string &publicPath, bool force) -> int
{
    if (!force && (std::filesystem::exists(privatePath) || std::filesystem::exists(publicPath)))
    {
        std::cerr << "key file exists (use --force to overwrite)\n";
        return 1;
    }

    auto keys = SZX::CRYPTO::generateRsa3072KeyPair();
    if (!keys.ok)
    {
        std::cerr << keys.message << '\n';
        return 1;
    }

    auto privatePem = SZX::CRYPTO::rsaPrivateKeyToPem(keys.value.privateKey);
    auto publicPem  = SZX::CRYPTO::rsaPublicKeyToPem(keys.value.publicKey);
    if (!privatePem.ok || !publicPem.ok)
    {
        std::cerr << "failed to encode generated key pair as PEM\n";
        return 1;
    }

    if (!writeBytes(privatePath, privatePem.value, force) || !writeBytes(publicPath, publicPem.value, force))
    {
        std::cerr << "failed to write key files (use --force to overwrite)\n";
        return 1;
    }

    auto id = SZX::CRYPTO::rsaPublicKeyId(keys.value.publicKey);
    if (id.ok)
        std::cout << "issuer_key_id=" << id.value << '\n';
    return 0;
}

auto runRequest(const std::string &product, const std::string &version, const std::string &publicKeyPath,
                const std::string &outputPath) -> int
{
    auto publicKey = readRsaKey(publicKeyPath, false);
    if (!publicKey)
    {
        std::cerr << "failed to read or parse issuer public key\n";
        return 1;
    }

    SZX::LICENSE::RequestOptions options{.productId = product, .appVersion = version};
    auto result = SZX::LICENSE::createRequestFile(options, outputPath, *publicKey);
    if (!result.ok)
    {
        std::cerr << "request generation failed: " << result.message << '\n';
        return 1;
    }
    return 0;
}

auto runIssue(const std::string &requestPath, const std::string &privateKeyPath, const std::string &publicKeyPath,
              const std::string &outputPath, const std::string &licenseId, const std::string &issuer,
              std::optional<std::int64_t> days) -> int
{
    auto privateKey = readRsaKey(privateKeyPath, true);
    auto publicKey  = readRsaKey(publicKeyPath, false);
    if (!privateKey || !publicKey)
    {
        std::cerr << "failed to read or parse issuer key pair\n";
        return 1;
    }

    SZX::LICENSE::IssueOptions options;
    options.licenseId =
        licenseId.empty() ? "license-" + std::to_string(SZX::COMMON::TimeHelper::NowTimestamp() / 1000) : licenseId;
    options.issuer = issuer.empty() ? "szx-license-issuer" : issuer;
    if (days)
    {
        options.expiresAtUtc = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + (*days * 86400);
    }

    auto result = SZX::LICENSE::issueFile(requestPath, options, *privateKey, *publicKey);
    if (!result.ok)
    {
        std::cerr << "license issue failed: " << result.message << '\n';
        return 1;
    }
    if (!writeBytes(outputPath, result.value))
    {
        std::cerr << "failed to write license (output may already exist)\n";
        return 1;
    }
    return 0;
}

auto runKeyInfo(const std::string &publicKeyPath) -> int
{
    auto publicKey = readRsaKey(publicKeyPath, false);
    if (!publicKey)
    {
        std::cerr << "failed to read or parse issuer public key\n";
        return 1;
    }

    auto id = SZX::CRYPTO::rsaPublicKeyId(*publicKey);
    if (!id.ok)
    {
        std::cerr << "failed to calculate issuer key ID: " << id.message << '\n';
        return 1;
    }
    std::cout << "issuer_key_id=" << id.value << '\n';
    return 0;
}

auto runVerify(const std::string &inputPath, const std::string &publicKeyPath, const std::string &expectedProduct) -> int
{
    auto publicKey = readRsaKey(publicKeyPath, false);
    if (!publicKey)
    {
        std::cerr << "failed to read or parse issuer public key\n";
        return 1;
    }

    auto result = SZX::LICENSE::verifyFile(inputPath, *publicKey, expectedProduct);
    if (!result.ok)
    {
        std::cerr << "license verification failed: " << result.message << '\n';
        return 1;
    }
    std::cout << "license_id=" << result.value.licenseId << '\n'
              << "product_id=" << result.value.productId << '\n'
              << "app_version=" << result.value.appVersion << '\n'
              << "issuer=" << result.value.issuer << '\n';
    return 0;
}

auto runMain(int argc, char **argv) -> int
{
    SZX::COMMON::Command command{"SZX license issuer"};
    command.set_help_flag("-h,--help", "Show this help message");
    command.require_subcommand(1);

    auto *keygen = command.add_subcommand("keygen", "Generate a 3072-bit RSA issuer key pair in PEM format");
    std::string keygenPrivatePath, keygenPublicPath;
    bool keygenForce = false;
    keygen->add_option("--private-key", keygenPrivatePath, "Output private key PEM path")->required();
    keygen->add_option("--public-key", keygenPublicPath, "Output public key PEM path")->required();
    keygen->add_flag("--force", keygenForce, "Overwrite existing key files");

    auto *request = command.add_subcommand("request", "Generate an encrypted license request file");
    std::string requestProduct, requestVersion, requestPublicKeyPath, requestOutputPath;
    request->add_option("--product", requestProduct, "Product identifier")->required();
    request->add_option("--version", requestVersion, "Application version")->required();
    request->add_option("--public-key", requestPublicKeyPath, "Issuer public key (DER or PEM)")->required();
    request->add_option("--output", requestOutputPath, "Output request file path")->required();

    auto *issue = command.add_subcommand("issue", "Decrypt a request and issue a signed license file");
    std::string issueRequestPath, issuePrivateKeyPath, issuePublicKeyPath, issueOutputPath, issueLicenseId, issueIssuer;
    std::int64_t issueDays = 0;
    issue->add_option("--request", issueRequestPath, "Input encrypted request file path")->required();
    issue->add_option("--private-key", issuePrivateKeyPath, "Issuer private key (DER or PEM)")->required();
    issue->add_option("--public-key", issuePublicKeyPath, "Issuer public key (DER or PEM)")->required();
    issue->add_option("--output", issueOutputPath, "Output license file path")->required();
    issue->add_option("--license-id", issueLicenseId, "License identifier");
    issue->add_option("--issuer", issueIssuer, "Issuer identifier");
    auto *issueDaysOption = issue->add_option("--days", issueDays, "Validity in days; omitted means permanent");
    issueDaysOption->check(CLI::Range(std::int64_t{1}, std::numeric_limits<std::int64_t>::max() / 86400));

    auto *keyInfo = command.add_subcommand("key-info", "Print the issuer public key ID");
    std::string keyInfoPublicKeyPath;
    keyInfo->add_option("--public-key", keyInfoPublicKeyPath, "Issuer public key (DER or PEM)")->required();

    auto *verify = command.add_subcommand("verify", "Verify a license file and print its information");
    std::string verifyInputPath, verifyPublicKeyPath, verifyExpectedProduct;
    verify->add_option("--input", verifyInputPath, "License file path")->required();
    verify->add_option("--public-key", verifyPublicKeyPath, "Issuer public key (DER or PEM)")->required();
    verify->add_option("--product", verifyExpectedProduct, "Expected product identifier");

    try
    {
        command.parse(argc, argv);
    } catch (const CLI::ParseError &error)
    {
        return command.exit(error);
    }

    if (keygen->parsed())
        return runKeygen(keygenPrivatePath, keygenPublicPath, keygenForce);
    if (request->parsed())
        return runRequest(requestProduct, requestVersion, requestPublicKeyPath, requestOutputPath);
    if (issue->parsed())
        return runIssue(issueRequestPath, issuePrivateKeyPath, issuePublicKeyPath, issueOutputPath, issueLicenseId, issueIssuer,
                        issueDaysOption->count() > 0 ? std::optional<std::int64_t>(issueDays) : std::nullopt);
    if (keyInfo->parsed())
        return runKeyInfo(keyInfoPublicKeyPath);
    if (verify->parsed())
        return runVerify(verifyInputPath, verifyPublicKeyPath, verifyExpectedProduct);
    return 2;
}
} // namespace

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
