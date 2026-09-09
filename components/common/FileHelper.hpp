/**
 * @file FileHelper.hpp
 * @brief 常用文件处理工具。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-09
 */

#pragma once

#include "FolderHelper.hpp"
#include "StringHelper.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#ifdef _WIN32
    #include <cwchar>
#endif

namespace SZX::COMMON
{

/**
 * @class FileHelper
 * @brief 提供文件存在性判断、读写、扩展名判断等无状态工具函数。
 */
class FileHelper final
{
public:
    static auto Exists(const std::filesystem::path &path) -> bool
    {
        std::error_code error;
        return std::filesystem::exists(path, error) && std::filesystem::is_regular_file(path, error);
    }

    static auto IsRegularFile(const std::filesystem::path &path) -> bool
    {
        return Exists(path);
    }

    /**
     * Compares two paths after resolving absolute components and existing
     * symlinks. Missing paths are compared lexically; Windows comparisons are
     * case-insensitive to match the native filesystem semantics.
     */
    static auto IsSamePath(const std::filesystem::path &left, const std::filesystem::path &right) -> bool
    {
        if (left.empty() || right.empty())
            return false;

        const auto normalize = [](const std::filesystem::path &path)
        {
            std::error_code error;
            const auto absolute = std::filesystem::absolute(path, error);
            if (error)
                return path.lexically_normal();
            error.clear();
            const auto canonical = std::filesystem::weakly_canonical(absolute, error);
            return (error ? absolute : canonical).lexically_normal();
        };
        const auto leftPath  = normalize(left);
        const auto rightPath = normalize(right);

        std::error_code error;
        if (std::filesystem::equivalent(leftPath, rightPath, error) && !error)
            return true;
#ifdef _WIN32
        return _wcsicmp(leftPath.native().c_str(), rightPath.native().c_str()) == 0;
#else
        return leftPath == rightPath;
#endif
    }

    static auto isSamePath(const std::filesystem::path &left, const std::filesystem::path &right) -> bool
    {
        return IsSamePath(left, right);
    }

    static auto Size(const std::filesystem::path &path) -> std::optional<std::uintmax_t>
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error)
            return std::nullopt;
        return size;
    }

    static auto Extension(const std::filesystem::path &path, bool lowerCase = true) -> std::string
    {
        auto extension = path.extension().string();
        if (lowerCase)
            extension = StringHelper::ToLower(extension);
        return extension;
    }

    static auto HasExtension(const std::filesystem::path &path, std::string_view extension, bool caseInsensitive = true) -> bool
    {
        auto actual   = path.extension().string();
        auto expected = NormalizeExtension(extension);
        if (caseInsensitive)
        {
            actual   = StringHelper::ToLower(actual);
            expected = StringHelper::ToLower(expected);
        }
        return actual == expected;
    }

    static auto HasExtension(const std::filesystem::path &path, const std::vector<std::string> &extensions,
                             bool caseInsensitive = true) -> bool
    {
        return std::any_of(extensions.begin(), extensions.end(),
                           [&](const auto &extension) { return HasExtension(path, extension, caseInsensitive); });
    }

    static auto ReadText(const std::filesystem::path &path) -> std::optional<std::string>
    {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!input.is_open())
            return std::nullopt;

        auto content = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (input.bad())
            return std::nullopt;
        return content;
    }

    static auto WriteText(const std::filesystem::path &path, std::string_view content, bool createParents = true) -> bool
    {
        if (createParents && !FolderHelper::EnsureParentExists(path))
            return false;

        std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            return false;

        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        return output.good();
    }

    static auto AppendText(const std::filesystem::path &path, std::string_view content, bool createParents = true) -> bool
    {
        if (createParents && !FolderHelper::EnsureParentExists(path))
            return false;

        std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::app);
        if (!output.is_open())
            return false;

        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        return output.good();
    }

    static auto ReadBytes(const std::filesystem::path &path) -> std::optional<std::vector<std::uint8_t>>
    {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!input.is_open())
            return std::nullopt;

        auto content = std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (input.bad())
            return std::nullopt;
        return content;
    }

    /** Reads a regular file with a hard upper bound, returning nullopt on overflow. */
    static auto ReadBytes(const std::filesystem::path &path, std::uintmax_t maxBytes) -> std::optional<std::vector<std::uint8_t>>
    {
        const auto size = Size(path);
        if (!size || *size > maxBytes || *size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
            return std::nullopt;
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!input.is_open())
            return std::nullopt;
        if (*size >= static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
            return std::nullopt;
        std::vector<std::uint8_t> content(static_cast<std::size_t>(*size) + 1);
        input.read(reinterpret_cast<char *>(content.data()), static_cast<std::streamsize>(content.size()));
        const auto count = input.gcount();
        if (count < 0 || static_cast<std::uintmax_t>(count) > maxBytes || input.bad())
            return std::nullopt;
        content.resize(static_cast<std::size_t>(count));
        return content;
    }

    static auto WriteBytes(const std::filesystem::path &path, const std::vector<std::uint8_t> &content, bool createParents = true)
        -> bool
    {
        if (createParents && !FolderHelper::EnsureParentExists(path))
            return false;

        std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            return false;

        if (!content.empty())
            output.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size()));
        return output.good();
    }

    static auto RemoveIfExists(const std::filesystem::path &path) -> bool
    {
        if (!Exists(path))
            return true;

        std::error_code error;
        return std::filesystem::remove(path, error) && !error;
    }

    static auto CopyFile(const std::filesystem::path &source, const std::filesystem::path &target, bool overwrite = true,
                         bool createParents = true) -> bool
    {
        if (!Exists(source))
            return false;

        if (createParents && !FolderHelper::EnsureParentExists(target))
            return false;

        std::error_code error;
        const auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;
        return std::filesystem::copy_file(source, target, options, error) && !error;
    }

private:
    static auto NormalizeExtension(std::string_view extension) -> std::string
    {
        if (extension.empty() || extension.front() == '.')
            return std::string(extension);
        return "." + std::string(extension);
    }
};

} // namespace SZX::COMMON
