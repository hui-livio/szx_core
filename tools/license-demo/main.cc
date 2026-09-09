#include "Command.hpp"
#include "FileHelper.hpp"
#include "SzxCrypto/Rsa.h"
#include "SzxLicense/License.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr std::uintmax_t kMaxInputFile = 2U * 1024U * 1024U;

auto readPublicKey(std::string_view path) -> std::optional<std::vector<std::uint8_t>>
{
    const auto filePath = std::filesystem::path(path);
    const auto size     = SZX::COMMON::FileHelper::Size(filePath);
    if (!size || *size > kMaxInputFile)
        return std::nullopt;
    auto bytes = SZX::COMMON::FileHelper::ReadBytes(filePath, kMaxInputFile);
    if (!bytes || bytes->empty())
        return std::nullopt;
    auto converted = SZX::CRYPTO::rsaPublicKeyFromEncoded(*bytes);
    return converted.ok ? std::optional<std::vector<std::uint8_t>>(std::move(converted.value)) : std::nullopt;
}

auto run(int argc, char **argv) -> int
{
    SZX::COMMON::Command command{"Client license request and verification demo"};
    command.set_help_flag("-h,--help", "Show this help message");
    command.require_subcommand(1);

    auto *request = command.add_subcommand("request", "Generate an encrypted license request file");
    std::string requestProduct, requestVersion, requestPublicKeyPath, requestOutputPath;
    request->add_option("--product", requestProduct, "Product identifier")->required();
    request->add_option("--version", requestVersion, "Application version")->required();
    request->add_option("--public-key", requestPublicKeyPath, "Issuer public key (DER or PEM)")->required();
    request->add_option("--output", requestOutputPath, "Output request file path")->required();

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
        const auto exitCode = command.exit(error);
        if (dynamic_cast<const CLI::CallForHelp *>(&error) != nullptr)
            return exitCode;
        return 2;
    }
    if (request->parsed())
    {
        auto key = readPublicKey(requestPublicKeyPath);
        if (!key)
        {
            std::cerr << "failed to read or parse issuer public key\n";
            return 1;
        }
        SZX::LICENSE::RequestOptions options{.productId = requestProduct, .appVersion = requestVersion};
        auto result = SZX::LICENSE::createRequestFile(options, requestOutputPath, *key);
        if (!result.ok)
        {
            std::cerr << "request generation failed: " << result.message << '\n';
            return 1;
        }
        std::cout << "request file generated: " << requestOutputPath << '\n'
                  << "send this file to the license issuer; no private key is used by this demo\n";
        return 0;
    }

    if (verify->parsed())
    {
        auto key = readPublicKey(verifyPublicKeyPath);
        if (!key)
        {
            std::cerr << "failed to read or parse issuer public key\n";
            return 1;
        }
        auto result = SZX::LICENSE::verifyFile(verifyInputPath, *key, verifyExpectedProduct);
        if (!result.ok)
        {
            std::cerr << "license verification failed: " << result.message << '\n';
            return 1;
        }
        const auto &license = result.value;
        std::cout << "license_id=" << license.licenseId << '\n'
                  << "product_id=" << license.productId << '\n'
                  << "app_version=" << license.appVersion << '\n'
                  << "issuer=" << license.issuer << '\n'
                  << "issued_at_utc=" << license.issuedAtUtc << '\n'
                  << "not_before_utc=" << license.notBeforeUtc << '\n'
                  << "expires_at_utc=";
        if (license.expiresAtUtc)
            std::cout << *license.expiresAtUtc;
        else
            std::cout << "permanent";
        std::cout << '\n' << "features=";
        for (std::size_t i = 0; i < license.features.size(); ++i)
        {
            if (i != 0)
                std::cout << ',';
            std::cout << license.features[i];
        }
        std::cout << '\n'
                  << "machine_factor_count=" << license.machineFactors.size() << '\n'
                  << "custom_field_count=" << license.customFields.size() << '\n';
        return 0;
    }
    return 2;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        return run(argc, argv);
    } catch (const std::exception &error)
    {
        std::cerr << "request demo failed: " << error.what() << "\n";
        return 1;
    } catch (...)
    {
        std::cerr << "request demo failed: unknown error\n";
        return 1;
    }
}
