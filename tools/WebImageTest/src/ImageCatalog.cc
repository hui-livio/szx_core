#include "ImageCatalog.h"

#include "FileHelper.hpp"
#include "FolderHelper.hpp"
#include "Serializer.hpp"

#include <algorithm>
#include <array>
#include <new>

namespace SZX::WEBIMAGE
{
inline namespace v1
{
namespace
{
template <typename T>
auto failure(ErrorCode code) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(code), {});
}
auto filenameUtf8(const std::filesystem::path &path) -> std::string
{
    const auto utf8 = path.filename().u8string();
    return {utf8.begin(), utf8.end()};
}
auto supported(const std::filesystem::path &path) -> bool
{
    constexpr std::array extensions{".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff", ".webp", ".ppm", ".pgm", ".pbm", ".pnm"};
    const auto extension = SZX::COMMON::FileHelper::Extension(path);
    return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
}
} // namespace

ImageCatalog::ImageCatalog(std::filesystem::path root, std::string defaultName) :
    root_(std::move(root)),
    defaultName_(std::move(defaultName))
{
}
ImageCatalog::~ImageCatalog()
{
    work_.close();
    // OpenCV decode cannot be cancelled midway; join before member storage is destroyed.
    if (worker_.joinable())
        worker_.join();
}

auto ImageCatalog::Create(const std::filesystem::path &directory, std::string defaultName)
    -> SZX::COMMON::Result<std::shared_ptr<ImageCatalog>>
{
    try
    {
        if (!SZX::COMMON::FolderHelper::IsDirectory(directory))
            return failure<std::shared_ptr<ImageCatalog>>(ErrorCode::DirectoryUnavailable);
        std::error_code error;
        const auto canonical = std::filesystem::canonical(directory, error);
        if (error)
            return failure<std::shared_ptr<ImageCatalog>>(ErrorCode::DirectoryUnavailable);
        auto result     = std::shared_ptr<ImageCatalog>(new ImageCatalog(canonical, std::move(defaultName)));
        result->worker_ = std::jthread([self = result.get()] { self->run(); });
        return SZX::COMMON::Result<std::shared_ptr<ImageCatalog>>::Success(std::move(result));
    } catch (const std::bad_alloc &)
    {
        return failure<std::shared_ptr<ImageCatalog>>(ErrorCode::ResourceExhausted);
    } catch (...)
    {
        return failure<std::shared_ptr<ImageCatalog>>(ErrorCode::UnexpectedFailure);
    }
}

auto ImageCatalog::resolve(const std::string &name) const -> SZX::COMMON::Result<std::filesystem::path>
{
    if (name.empty() || name == "." || name == ".." || name.size() > 1024 || name.find_first_of("/\\:") != name.npos ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        return failure<std::filesystem::path>(ErrorCode::InvalidImageName);
    const auto leaf = std::filesystem::path(std::u8string(name.begin(), name.end()));
    if (!supported(leaf))
        return failure<std::filesystem::path>(ErrorCode::InvalidImageName);
    std::error_code error;
    const auto path   = root_ / leaf;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || !std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
        return failure<std::filesystem::path>(ErrorCode::ImageNotFound);
    const auto canonical = std::filesystem::canonical(path, error);
    if (error || canonical.parent_path() != root_)
        return failure<std::filesystem::path>(ErrorCode::InvalidImageName);
    return SZX::COMMON::Result<std::filesystem::path>::Success(canonical);
}

auto ImageCatalog::list() const -> SZX::COMMON::Result<std::string>
{
    try
    {
        SZX::COMMON::Json items = SZX::COMMON::Json::array();
        // FolderHelper::ListFiles intentionally returns partial/empty results on IO failure.
        // This API must distinguish a genuinely empty directory from enumeration failure.
        std::error_code error;
        std::filesystem::directory_iterator current(root_, error), end;
        if (error)
            return failure<std::string>(ErrorCode::DirectoryUnavailable);
        for (; current != end; current.increment(error))
        {
            if (error)
                return failure<std::string>(ErrorCode::DirectoryUnavailable);
            const auto status = current->symlink_status(error);
            if (error)
                return failure<std::string>(ErrorCode::DirectoryUnavailable);
            if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status) || !supported(current->path()))
                continue;
            const auto name = filenameUtf8(current->path());
            if (!resolve(name).ok)
                continue;
            const auto size = SZX::COMMON::FileHelper::Size(current->path());
            if (!size)
                continue;
            if (items.size() >= 10000)
                return failure<std::string>(ErrorCode::DirectoryLimit);
            items.push_back(
                {{"name", name}, {"size_bytes", *size}, {"loadable", *size > 0 && *size <= ImageSnapshot::MaxFileBytes}});
        }
        if (error)
            return failure<std::string>(ErrorCode::DirectoryUnavailable);
        std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) { return a.at("name") < b.at("name"); });
        std::string selected;
        for (const auto &item : items)
        {
            if (!item.at("loadable").template get<bool>())
                continue;
            const auto name = item.at("name").template get<std::string>();
            if (selected.empty() || name == defaultName_)
                selected = name;
            if (name == defaultName_)
                break;
        }
        return SZX::COMMON::Result<std::string>::Success(
            SZX::COMMON::Json{{"items", std::move(items)}, {"default", selected}}.dump());
    } catch (const std::bad_alloc &)
    {
        return failure<std::string>(ErrorCode::ResourceExhausted);
    } catch (...)
    {
        return failure<std::string>(ErrorCode::UnexpectedFailure);
    }
}

auto ImageCatalog::request(std::string name, bool allowLoad) -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    using Value = std::shared_ptr<const ImageSnapshot>;
    try
    {
        if (name.empty())
            name = defaultName_;
        if (name.empty() || name == "." || name == ".." || name.size() > 1024 || name.find_first_of("/\\:") != name.npos ||
            std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }) ||
            !supported(std::filesystem::path(std::u8string(name.begin(), name.end()))))
            return failure<Value>(ErrorCode::InvalidImageName);
        // Keep a consistent cached snapshot after file removal; reloading requires a listed file.
        {
            std::lock_guard guard(mutex_);
            if (cached_ && cachedName_ == name)
                return SZX::COMMON::Result<Value>::Success(cached_);
            if (!allowLoad)
            {
                if (loading_ == name)
                    return failure<Value>(ErrorCode::ImageLoading);
                if (failedName_ == name)
                    return SZX::COMMON::Result<Value>::Failure(failure_, {});
                return failure<Value>(ErrorCode::ImageNotLoaded);
            }
        }
        const auto path = resolve(name);
        if (!path.ok)
            return SZX::COMMON::Result<Value>::Failure(path.error, {});
        std::lock_guard guard(mutex_);
        if (cached_ && cachedName_ == name)
            return SZX::COMMON::Result<Value>::Success(cached_);
        if (!loading_.empty())
            return failure<Value>(loading_ == name ? ErrorCode::ImageLoading : ErrorCode::ImageBusy);
        // Queue publication under the same lock prevents a worker racing the state update.
        if (work_.try_push(name) != SZX::COMMON::QueuePushResult::Pushed)
            return failure<Value>(ErrorCode::ImageBusy);
        loading_ = std::move(name);
        failedName_.clear();
        failure_ = 0;
        return failure<Value>(ErrorCode::ImageLoading);
    } catch (const std::bad_alloc &)
    {
        return failure<Value>(ErrorCode::ResourceExhausted);
    } catch (...)
    {
        return failure<Value>(ErrorCode::UnexpectedFailure);
    }
}

auto ImageCatalog::load(std::string name) -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    return request(std::move(name), true);
}
auto ImageCatalog::get(std::string name) -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
{
    return request(std::move(name), false);
}

void ImageCatalog::run() noexcept
{
    try
    {
        std::string name;
        while (work_.pop(name))
        {
            std::shared_ptr<const ImageSnapshot> retired;
            {
                std::lock_guard guard(mutex_);
                retired = std::move(cached_);
                cachedName_.clear();
            }
            retired.reset(); // Potentially large deallocation stays outside the catalog lock.
            auto loaded = [&]() -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>
            {
                try
                {
                    const auto path = resolve(name);
                    if (!path.ok)
                        return SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>::Failure(path.error, {});
                    return ImageSnapshot::Load(path.value);
                } catch (const std::bad_alloc &)
                {
                    return failure<std::shared_ptr<const ImageSnapshot>>(ErrorCode::ResourceExhausted);
                } catch (...)
                {
                    return failure<std::shared_ptr<const ImageSnapshot>>(ErrorCode::UnexpectedFailure);
                }
            }();
            std::lock_guard guard(mutex_);
            loading_.clear();
            if (loaded.ok)
            {
                cached_     = std::move(loaded.value);
                cachedName_ = std::move(name);
                failedName_.clear();
                failure_ = 0;
            }
            else
            {
                failedName_ = std::move(name);
                failure_    = loaded.error;
            }
        }
    } catch (...)
    {
        // Last-resort containment: no exception may escape the worker thread.
        try
        {
            std::lock_guard guard(mutex_);
            failedName_ = std::move(loading_);
            failure_    = static_cast<int>(ErrorCode::UnexpectedFailure);
        } catch (...)
        {
        }
        work_.close();
    }
}
} // namespace v1
} // namespace SZX::WEBIMAGE
