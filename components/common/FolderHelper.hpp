/**
 * @file FolderHelper.hpp
 * @brief 常用目录处理工具。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-09
 */

#pragma once

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

namespace SZX::COMMON
{

/**
 * @class FolderHelper
 * @brief 提供目录存在性判断、创建、枚举等无状态工具函数。
 */
class FolderHelper final
{
public:
    static auto Exists(const std::filesystem::path &path) -> bool
    {
        std::error_code error;
        return std::filesystem::exists(path, error) && std::filesystem::is_directory(path, error);
    }

    static auto IsDirectory(const std::filesystem::path &path) -> bool
    {
        return Exists(path);
    }

    static auto EnsureExists(const std::filesystem::path &path) -> bool
    {
        if (path.empty())
            return false;

        std::error_code error;
        if (std::filesystem::exists(path, error))
            return std::filesystem::is_directory(path, error);

        return std::filesystem::create_directories(path, error) && !error;
    }

    static auto EnsureParentExists(const std::filesystem::path &path) -> bool
    {
        const auto parent = path.parent_path();
        return parent.empty() || EnsureExists(parent);
    }

    static auto IsEmpty(const std::filesystem::path &path) -> bool
    {
        std::error_code error;
        return std::filesystem::is_empty(path, error) && !error;
    }

    static auto ListFiles(const std::filesystem::path &path, bool recursive = false) -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> files;
        if (!Exists(path))
            return files;

        if (recursive)
        {
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator
                     it(path, std::filesystem::directory_options::skip_permission_denied, error),
                 end;
                 it != end && !error; it.increment(error))
            {
                std::error_code entryError;
                if (it->is_regular_file(entryError) && !entryError)
                    files.emplace_back(it->path());
            }
        }
        else
        {
            std::error_code error;
            for (std::filesystem::directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, error),
                 end;
                 it != end && !error; it.increment(error))
            {
                std::error_code entryError;
                if (it->is_regular_file(entryError) && !entryError)
                    files.emplace_back(it->path());
            }
        }

        SortPaths(files);
        return files;
    }

    static auto ListDirectories(const std::filesystem::path &path) -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> directories;
        if (!Exists(path))
            return directories;

        std::error_code error;
        for (std::filesystem::directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end && !error; it.increment(error))
        {
            std::error_code entryError;
            if (it->is_directory(entryError) && !entryError)
                directories.emplace_back(it->path());
        }

        SortPaths(directories);
        return directories;
    }

    static auto Absolute(const std::filesystem::path &path) -> std::filesystem::path
    {
        std::error_code error;
        auto result = std::filesystem::absolute(path, error);
        return error ? path : result;
    }

    static auto WeaklyCanonical(const std::filesystem::path &path) -> std::filesystem::path
    {
        std::error_code error;
        auto result = std::filesystem::weakly_canonical(path, error);
        return error ? path.lexically_normal() : result;
    }

private:
    static void SortPaths(std::vector<std::filesystem::path> &paths)
    {
        std::sort(paths.begin(), paths.end(),
                  [](const auto &lhs, const auto &rhs) { return lhs.generic_string() < rhs.generic_string(); });
    }
};

} // namespace SZX::COMMON
