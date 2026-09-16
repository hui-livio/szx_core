#pragma once

#include "SzxNetwork/HttpServer.h"

namespace SZX::NETWORK::v1::detail
{
// Private adapter. libhv owns sockets/loops; its C HTTP parser is fed directly
// so length checks run before allocation and every C callback contains exceptions.
class HttpTransport final
{
public:
    using Handler      = std::function<HttpServerResponse(HttpServerRequest &)>;
    using ErrorHandler = std::function<void(NetworkError)>;

    HttpTransport(HttpServerOptions options, Handler handler, ErrorHandler onError);
    ~HttpTransport();
    HttpTransport(const HttpTransport &)                     = delete;
    auto operator=(const HttpTransport &) -> HttpTransport & = delete;

    [[nodiscard]] auto start() -> SZX::COMMON::Result<std::uint16_t>;
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace SZX::NETWORK::v1::detail
