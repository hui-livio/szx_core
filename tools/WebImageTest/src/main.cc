#include "ImageCatalog.h"
#include "ImageService.h"

#include "Command.hpp"
#include "FolderHelper.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <new>
#include <string>
#include <thread>

namespace
{
volatile std::sig_atomic_t stopRequested = 0;
void requestStop(int) noexcept
{
    stopRequested = 1;
}
int report(int code)
{
    std::cerr << "WebImageTest failed, code=" << code << '\n';
    return 1;
}
} // namespace

int main(int argc, char **argv)
{
    using namespace SZX::WEBIMAGE;
    try
    {
        const auto base       = SZX::COMMON::FolderHelper::Absolute(argv[0]).parent_path() / "web-image";
        std::string imagePath = "/mnt/d/share/images/moban.png";
        std::string imagesDirectory;
        std::string webRoot = (base / "web").string();
        std::string host    = "127.0.0.1";
        unsigned int port   = 7380;
        SZX::COMMON::Command command{"Plain HTTP/OpenCV image viewer (no Vue, no database)", "szx_web_image_test"};
        command.add_option("--image", imagePath,
                           "Initially selected image; decoded on demand, defines the default images directory");
        command.add_option("--images-dir", imagesDirectory, "Image catalog directory (default: parent of --image)");
        command.add_option("--web-root", webRoot, "Static web directory");
        command.add_option("--host", host, "Listen address (default: loopback)");
        command.add_option("--port", port, "Listen port, 0 selects an available port")->check(CLI::Range(0U, 65535U));
        CMD_PARSE(command, argc, argv);
        const auto imageFile   = SZX::COMMON::FolderHelper::Absolute(std::filesystem::path(imagePath));
        const auto root        = imagesDirectory.empty() ? imageFile.parent_path() : std::filesystem::path(imagesDirectory);
        const auto defaultUtf8 = imageFile.filename().u8string();
        const auto catalog     = ImageCatalog::Create(root, std::string(defaultUtf8.begin(), defaultUtf8.end()));
        if (!catalog.ok)
            return report(catalog.error);
        auto created = SZX::NETWORK::HttpServer::Create(
            {.listen = {host, static_cast<std::uint16_t>(port)}, .ioThreadCount = 2, .maxBodyBytes = 1024});
        if (!created.ok)
            return report(static_cast<int>(ErrorCode::NetworkConfigurationFailed));
        auto server        = std::move(created.value);
        const auto limited = server->setMaxConnections(16);
        if (!limited.ok)
            return report(static_cast<int>(ErrorCode::NetworkConfigurationFailed));
        const auto registered = RegisterCatalogRoutes(*server, catalog.value, std::filesystem::path(webRoot));
        if (!registered.ok)
            return report(registered.error);
        std::signal(SIGINT, requestStop);
        std::signal(SIGTERM, requestStop);
        const auto started = server->start();
        if (!started.ok)
            return report(static_cast<int>(ErrorCode::ListenFailed));
        std::cout << "WebImageTest ready: http://" << host << ':' << server->port() << "/\n"
                  << "Image directory ready. Images are decoded on demand by a single worker." << std::endl;
        while (!stopRequested)
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        const auto stopped = server->stop();
        return stopped.ok ? 0 : report(static_cast<int>(ErrorCode::UnexpectedFailure));
    } catch (const std::bad_alloc &)
    {
        return report(static_cast<int>(ErrorCode::ResourceExhausted));
    } catch (...)
    {
        return report(static_cast<int>(ErrorCode::UnexpectedFailure));
    }
}
