/**
 * @file FileFolderHelper_test.cc
 * @brief FileFolderHelper_test.cc 覆盖的单元测试与测试辅助。
 */

#include "FileHelper.hpp"
#include "FolderHelper.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{
using namespace SZX::COMMON;

auto MakeTempRoot() -> std::filesystem::path
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("szx_common_file_helper_test_" + std::to_string(suffix));
}

class common_FileFolderHelperTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root_ = MakeTempRoot();
        ASSERT_TRUE(FolderHelper::EnsureExists(root_));
    }

    void TearDown() override
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    std::filesystem::path root_;
};

TEST_F(common_FileFolderHelperTest, FolderHelperCreatesAndListsDirectories)
{
    const auto nested = root_ / "a" / "b";
    EXPECT_TRUE(FolderHelper::EnsureExists(nested));
    EXPECT_TRUE(FolderHelper::Exists(nested));
    EXPECT_TRUE(FolderHelper::EnsureParentExists(nested / "file.txt"));

    ASSERT_TRUE(FileHelper::WriteText(root_ / "a" / "one.txt", "1"));
    ASSERT_TRUE(FileHelper::WriteText(root_ / "a" / "b" / "two.txt", "2"));

    const auto topFiles = FolderHelper::ListFiles(root_ / "a");
    ASSERT_EQ(topFiles.size(), 1);
    EXPECT_EQ(topFiles[0].filename().string(), "one.txt");

    const auto recursiveFiles = FolderHelper::ListFiles(root_ / "a", true);
    ASSERT_EQ(recursiveFiles.size(), 2);
    auto recursiveFileNames = std::vector<std::string>{
        recursiveFiles[0].filename().string(),
        recursiveFiles[1].filename().string(),
    };
    std::sort(recursiveFileNames.begin(), recursiveFileNames.end());
    EXPECT_EQ(recursiveFileNames[0], "one.txt");
    EXPECT_EQ(recursiveFileNames[1], "two.txt");

    const auto directories = FolderHelper::ListDirectories(root_ / "a");
    ASSERT_EQ(directories.size(), 1);
    EXPECT_EQ(directories[0].filename().string(), "b");
}

TEST_F(common_FileFolderHelperTest, FileHelperReadsWritesAndAppendsText)
{
    const auto filePath = root_ / "nested" / "sample.txt";
    EXPECT_FALSE(FileHelper::Exists(filePath));

    ASSERT_TRUE(FileHelper::WriteText(filePath, "hello"));
    EXPECT_TRUE(FileHelper::Exists(filePath));
    EXPECT_EQ(FileHelper::ReadText(filePath), std::optional<std::string>("hello"));

    ASSERT_TRUE(FileHelper::AppendText(filePath, " world"));
    EXPECT_EQ(FileHelper::ReadText(filePath), std::optional<std::string>("hello world"));
    ASSERT_TRUE(FileHelper::Size(filePath).has_value());
    EXPECT_EQ(*FileHelper::Size(filePath), 11U);
}

TEST_F(common_FileFolderHelperTest, FileHelperReadsWritesBytes)
{
    const auto filePath = root_ / "bytes.bin";
    const auto content  = std::vector<std::uint8_t>{0, 1, 2, 255};

    ASSERT_TRUE(FileHelper::WriteBytes(filePath, content));
    const auto loaded = FileHelper::ReadBytes(filePath);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, content);
}

TEST_F(common_FileFolderHelperTest, FileHelperComparesEquivalentPaths)
{
    const auto existing = root_ / "same.txt";
    ASSERT_TRUE(FileHelper::WriteText(existing, "same"));
    ASSERT_TRUE(FolderHelper::EnsureExists(root_ / "folder"));

    EXPECT_TRUE(FileHelper::IsSamePath(existing, root_ / "folder" / ".." / "same.txt"));
    EXPECT_TRUE(FileHelper::isSamePath(existing, root_ / "same.txt"));
    EXPECT_TRUE(FileHelper::IsSamePath(root_ / "folder" / ".." / "future.txt", root_ / "future.txt"));
    EXPECT_FALSE(FileHelper::IsSamePath(existing, root_ / "other.txt"));
    EXPECT_FALSE(FileHelper::IsSamePath({}, existing));
}

TEST_F(common_FileFolderHelperTest, FileHelperHandlesExtensionsAndRemove)
{
    const auto filePath = root_ / "Image.PNG";
    ASSERT_TRUE(FileHelper::WriteText(filePath, "data"));

    EXPECT_EQ(FileHelper::Extension(filePath), ".png");
    EXPECT_TRUE(FileHelper::HasExtension(filePath, ".png"));
    EXPECT_TRUE(FileHelper::HasExtension(filePath, "PNG"));
    EXPECT_TRUE(FileHelper::HasExtension(filePath, std::vector<std::string>{".jpg", ".png"}));
    EXPECT_FALSE(FileHelper::HasExtension(filePath, ".jpg"));

    EXPECT_TRUE(FileHelper::RemoveIfExists(filePath));
    EXPECT_FALSE(FileHelper::Exists(filePath));
    EXPECT_TRUE(FileHelper::RemoveIfExists(filePath));
}

TEST_F(common_FileFolderHelperTest, FileHelperCopiesFileAndCreatesParentFolder)
{
    const auto sourcePath = root_ / "source.txt";
    const auto targetPath = root_ / "copy" / "target.txt";

    ASSERT_TRUE(FileHelper::WriteText(sourcePath, "copy content"));
    EXPECT_TRUE(FileHelper::CopyFile(sourcePath, targetPath));
    EXPECT_EQ(FileHelper::ReadText(targetPath), std::optional<std::string>("copy content"));

    ASSERT_TRUE(FileHelper::WriteText(sourcePath, "new content"));
    EXPECT_FALSE(FileHelper::CopyFile(sourcePath, targetPath, false));
    EXPECT_EQ(FileHelper::ReadText(targetPath), std::optional<std::string>("copy content"));

    EXPECT_TRUE(FileHelper::CopyFile(sourcePath, targetPath));
    EXPECT_EQ(FileHelper::ReadText(targetPath), std::optional<std::string>("new content"));
}

} // namespace
