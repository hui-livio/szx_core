#pragma once

#include "BlockQueue.hpp"
#include "ImageService.h"

#include <mutex>
#include <thread>

namespace SZX::WEBIMAGE
{
inline namespace v1
{
/** Directory-confined lazy loader: one worker, one queued item, one retained snapshot. */
class ImageCatalog final
{
public:
    [[nodiscard]] static auto Create(const std::filesystem::path &directory, std::string defaultName)
        -> SZX::COMMON::Result<std::shared_ptr<ImageCatalog>>;
    ~ImageCatalog();
    ImageCatalog(const ImageCatalog &)                     = delete;
    auto operator=(const ImageCatalog &) -> ImageCatalog & = delete;
    /** Enumerate immediate regular images, excluding symlinks and unsupported extensions. */
    [[nodiscard]] auto list() const -> SZX::COMMON::Result<std::string>;
    /** Queue decode on metadata requests only; PNG/tile requests never switch the cache. */
    [[nodiscard]] auto get(std::string name, bool allowLoad, bool retry = false)
        -> SZX::COMMON::Result<std::shared_ptr<const ImageSnapshot>>;

private:
    ImageCatalog(std::filesystem::path root, std::string defaultName);
    [[nodiscard]] auto resolve(const std::string &name) const -> SZX::COMMON::Result<std::filesystem::path>;
    void run() noexcept;
    std::filesystem::path root_;
    std::string defaultName_;
    std::mutex mutex_;
    std::string cachedName_;
    std::shared_ptr<const ImageSnapshot> cached_;
    std::string loading_;
    std::string failedName_;
    int failure_ = 0;
    SZX::COMMON::BlockingQueue<std::string> work_{1};
    std::jthread worker_;
};
} // namespace v1
} // namespace SZX::WEBIMAGE
