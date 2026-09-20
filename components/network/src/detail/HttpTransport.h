#pragma once

#include "SzxNetwork/HttpServer.h"

namespace SZX::NETWORK::v1::detail
{
// 私有传输适配器：libhv 管理套接字与事件循环，直接向其 C HTTP 解析器输入数据。
// 在分配报文体之前检查长度，所有 C 回调均在内部拦截 C++ 异常。
class HttpTransport final
{
public:
    using Handler      = std::function<HttpServerResponse(HttpServerRequest &)>;
    using ErrorHandler = std::function<void(NetworkError)>;

    HttpTransport(HttpServerOptions options, std::uint32_t maxConnections, Handler handler, ErrorHandler onError,
                  HttpRequestErrorHandler onRequestError);
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
