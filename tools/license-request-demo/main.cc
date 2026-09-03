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
    const std::string_view content(reinterpret_cast<const char *>(bytes->data()), bytes->size());
    if (content.find("-----BEGIN") != std::string_view::npos)
    {
        auto converted = SZX::CRYPTO::rsaPublicKeyFromPem(*bytes);
        if (!converted.ok)
            return std::nullopt;
        return std::move(converted.value);
    }
    return std::move(*bytes);
}

auto run(int argc, char **argv) -> int
{
    SZX::COMMON::Command command{"Client license request file demo"};
    command.set_help_flag("-h,--help", "Show this help message");
    std::string product;
    std::string version;
    std::string publicKeyPath;
    std::string outputPath;
    command.add_option("--product", product, "Product identifier")->required();
    command.add_option("--version", version, "Application version")->required();
    command.add_option("--public-key", publicKeyPath, "Issuer public key (DER or PEM)")->required();
    command.add_option("--output", outputPath, "Output request file path")->required();
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
    if (product.empty() || version.empty() || publicKeyPath.empty() || outputPath.empty())
    {
        std::cerr << "product, version, public-key and output must not be empty\n";
        return 2;
    }
    auto key = readPublicKey(publicKeyPath);
    if (!key)
    {
        std::cerr << "failed to read or parse issuer public key\n";
        return 1;
    }
    SZX::LICENSE::RequestOptions options;
    options.productId  = product;
    options.appVersion = version;
    auto result        = SZX::LICENSE::createRequestFile(options, outputPath, *key);
    if (!result.ok)
    {
        std::cerr << "request generation failed: " << result.message << "\n";
        return 1;
    }
    std::cout << "request file generated: " << outputPath << "\n"
              << "send this file to the license issuer; no private key is used by this demo\n";
    return 0;
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
