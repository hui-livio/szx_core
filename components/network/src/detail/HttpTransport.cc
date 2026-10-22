#include "HttpTransport.h"

#include "http_parser.h"
#include "hv/HttpMessage.h"
#include "hv/TcpServer.h"

#include <atomic>
#include <climits>
#include <utility>

namespace SZX::NETWORK::v1::detail
{
namespace
{
auto equalAscii(std::string_view left, std::string_view right) noexcept -> bool
{
    const CaseInsensitiveLess less;
    return !less(left, right) && !less(right, left);
}
} // namespace

class HttpTransport::Impl final
{
public:
    class Connection;
    struct Channel final : hv::SocketChannel
    {
        explicit Channel(hio_t *io) :
            hv::SocketChannel(io)
        {
        }
        std::unique_ptr<Connection> http;
        bool admitted = false;
    };

    class Connection final
    {
    public:
        Connection(Impl &owner, Channel &channel) :
            owner_(owner),
            channel_(channel)
        {
            http_parser_init(&parser_, HTTP_REQUEST);
            parser_.data = this;
            char ip[64]{};
            auto *address = reinterpret_cast<sockaddr_u *>(hio_peeraddr(channel.io()));
            sockaddr_ip(address, ip, sizeof(ip));
            peer_ = {ip, sockaddr_port(address)};
        }

        void receive(const char *bytes, std::size_t size) noexcept
        {
            if (stopped_)
                return;
            const auto parsed = http_parser_execute(&parser_, &settings(), bytes, size);
            if (!stopped_ && (parsed != size || HTTP_PARSER_ERRNO(&parser_) != HPE_OK))
            {
                fail(HTTP_PARSER_ERRNO(&parser_) == HPE_HEADER_OVERFLOW ? 431 : 400, NetworkError::ProtocolError);
            }
        }

    private:
        // All parser hooks go through this boundary, including URL/header assembly.
        template <typename Fn>
        static auto callback(http_parser *parser, Fn &&fn) noexcept -> int
        {
            auto &connection = *static_cast<Connection *>(parser->data);
            if (connection.stopped_)
                return -1;
            try
            {
                fn(connection);
            } catch (...)
            {
                connection.fail(500, NetworkError::Internal);
            }
            return connection.stopped_ ? -1 : 0;
        }

        static auto settings() noexcept -> const http_parser_settings &
        {
            static const http_parser_settings value = []
            {
                http_parser_settings result{};
                result.on_message_begin = [](http_parser *parser) noexcept
                {
                    return callback(parser,
                                    [](Connection &c)
                                    {
                                        c.request_      = {};
                                        c.request_.peer = c.peer_;
                                        c.target_.clear();
                                        c.field_.clear();
                                        c.value_.clear();
                                        c.readingValue_ = false;
                                    });
                };
                result.on_url = [](http_parser *parser, const char *data, std::size_t size) noexcept
                {
                    return callback(parser, [&](Connection &c) { c.target_.append(data, size); });
                };
                result.on_header_field = [](http_parser *parser, const char *data, std::size_t size) noexcept
                {
                    return callback(parser,
                                    [&](Connection &c)
                                    {
                                        // Trailers must not overwrite request headers used for authentication/framing.
                                        if (parser->flags & F_TRAILING)
                                            return;
                                        if (c.readingValue_)
                                            c.finishHeader();
                                        c.field_.append(data, size);
                                    });
                };
                result.on_header_value = [](http_parser *parser, const char *data, std::size_t size) noexcept
                {
                    return callback(parser,
                                    [&](Connection &c)
                                    {
                                        if (parser->flags & F_TRAILING)
                                            return;
                                        c.readingValue_ = true;
                                        c.value_.append(data, size);
                                    });
                };
                result.on_headers_complete = [](http_parser *parser) noexcept
                {
                    return callback(parser, [](Connection &c) { c.headersComplete(); });
                };
                result.on_chunk_header = [](http_parser *parser) noexcept
                {
                    return callback(parser,
                                    [&](Connection &c)
                                    {
                                        // Compare uint64_t before any narrowing, reserve or body read.
                                        if (parser->content_length > c.owner_.options.maxBodyBytes - c.request_.body.size())
                                            c.fail(413, NetworkError::MessageTooLarge);
                                    });
                };
                result.on_body = [](http_parser *parser, const char *data, std::size_t size) noexcept
                {
                    return callback(parser,
                                    [&](Connection &c)
                                    {
                                        if (size > c.owner_.options.maxBodyBytes - c.request_.body.size())
                                            c.fail(413, NetworkError::MessageTooLarge);
                                        else
                                            c.request_.body.append(data, size);
                                    });
                };
                result.on_message_complete = [](http_parser *parser) noexcept
                {
                    return callback(parser,
                                    [](Connection &c)
                                    {
                                        auto response = c.owner_.handler(c.request_);
                                        c.send(std::move(response), http_should_keep_alive(&c.parser_) != 0);
                                    });
                };
                return result;
            }();
            return value;
        }

        void finishHeader()
        {
            if (!field_.empty())
                request_.headers[std::move(field_)] = std::move(value_);
            field_.clear();
            value_.clear();
            readingValue_ = false;
        }

        void headersComplete()
        {
            finishHeader();
            request_.method = http_method_str(static_cast<http_method>(parser_.method));
            if ((parser_.flags & F_CONTENTLENGTH) && parser_.content_length > owner_.options.maxBodyBytes)
            {
                fail(413, NetworkError::MessageTooLarge);
                return;
            }
            if (parser_.upgrade || request_.headers.contains("Upgrade"))
            {
                fail(400, NetworkError::ProtocolError);
                return;
            }
            std::string_view resource = target_;
            if (!resource.starts_with('/') && resource != "*")
            {
                // The request parser already validates absolute-form targets. Keep the
                // authority separate; :// inside an origin-form path is ordinary text.
                const auto scheme = resource.find("://");
                if (scheme == resource.npos)
                {
                    fail(400, NetworkError::ProtocolError);
                    return;
                }
                const auto begin = resource.find_first_of("/?#", scheme + 3);
                resource         = begin == resource.npos ? std::string_view{} : resource.substr(begin);
            }
            resource              = resource.substr(0, resource.find('#'));
            const auto queryStart = resource.find('?');
            const auto path       = resource.substr(0, queryStart);
            request_.path         = path.empty() ? "/" : std::string{path};
            if (queryStart != resource.npos)
            {
                const std::string query{resource.substr(queryStart + 1)};
                hv::QueryParams parsed;
                hv::parse_query_params(query.c_str(), parsed);
                request_.query.insert(parsed.begin(), parsed.end());
            }
            const auto expect = request_.headers.find("Expect");
            if (expect != request_.headers.end())
            {
                if (!equalAscii(expect->second, "100-continue"))
                    fail(417, NetworkError::ProtocolError);
                else
                    write("HTTP/1.1 100 Continue\r\n\r\n");
            }
        }

        void write(const std::string &data)
        {
            if (hio_write(channel_.io(), data.data(), data.size()) < 0)
            {
                owner_.report(NetworkError::SendFailed);
                close();
            }
        }

        void send(HttpServerResponse response, bool keepAlive)
        {
            ::HttpResponse output;
            output.status_code = static_cast<http_status>(response.statusCode);
            if (response.statusCode != 204 && response.statusCode != 205 && response.statusCode != 304)
                output.body = std::move(response.body);
            for (const auto &[name, value] : response.headers)
                if (!equalAscii(name, "Content-Length") && !equalAscii(name, "Transfer-Encoding") &&
                    !equalAscii(name, "Connection"))
                    output.SetHeader(name.c_str(), value);
            if (response.statusCode != 204 && response.statusCode != 304)
                output.SetHeader("Content-Length", std::to_string(output.body.size()));
            output.SetHeader("Connection", keepAlive ? "keep-alive" : "close");
            // HEAD has the same representation headers but never sends a body.
            // Returning normally lets the incremental parser begin the next pipelined request.
            const auto headers   = output.Dump(true, false);
            const auto bodyBytes = parser_.method == HTTP_HEAD ? std::size_t{0} : output.body.size();
            if (headers.size() > INT_MAX || bodyBytes > INT_MAX - headers.size())
            {
                owner_.report(NetworkError::MessageTooLarge);
                close();
                return;
            }
            // libhv defaults to a 16 MiB write queue. Allow one complete response, including
            // its headers, without making pipelined/slow-reader backlogs unbounded.
            const auto responseBytes = static_cast<std::uint32_t>(headers.size() + bodyBytes);
            if (responseBytes > maxWriteBufferBytes_)
            {
                maxWriteBufferBytes_ = responseBytes;
                channel_.setMaxWriteBufsize(maxWriteBufferBytes_);
            }
            write(headers);
            if (!stopped_ && bodyBytes != 0)
                write(output.body);
            if (!keepAlive)
                close();
        }

        void close() noexcept
        {
            if (stopped_)
                return;
            stopped_ = true;
            channel_.stopRead();
            channel_.close(true); // Queue close; libhv drains pending writes before freeing the channel.
        }

        void fail(int status, NetworkError error) noexcept
        {
            if (stopped_)
                return;
            try
            {
                send(owner_.requestErrorHandler(request_, status, error), false);
            } catch (...)
            {
                close();
            }
        }

        Impl &owner_;
        Channel &channel_;
        http_parser parser_{};
        Endpoint peer_;
        HttpServerRequest request_;
        std::string target_;
        std::string field_;
        std::string value_;
        bool readingValue_                 = false;
        bool stopped_                      = false;
        std::uint32_t maxWriteBufferBytes_ = 16U * 1024U * 1024U;
    };

    Impl(HttpServerOptions value, std::uint32_t maxConnections, Handler callback, ErrorHandler onError,
         HttpRequestErrorHandler onRequestError) :
        options(std::move(value)),
        handler(std::move(callback)),
        errorHandler(std::move(onError)),
        requestErrorHandler(std::move(onRequestError)),
        connectionLimit(maxConnections)
    {
        server.setThreadNum(static_cast<int>(options.ioThreadCount - 1));
        server.onConnection = [this](const std::shared_ptr<Channel> &incoming) noexcept
        {
            const auto channel = incoming;
            if (!channel->isConnected())
            {
                if (channel->admitted)
                    connectionCount.fetch_sub(1);
                return;
            }
            if (connectionCount.fetch_add(1) >= connectionLimit)
            {
                connectionCount.fetch_sub(1);
                channel->close(true);
                return;
            }
            channel->admitted = true;
            try
            {
                channel->http = std::make_unique<Connection>(*this, *channel);
                channel->setKeepaliveTimeout(static_cast<int>(options.keepaliveTimeoutMs));
            } catch (...)
            {
                report(NetworkError::Internal);
                channel->close(true);
            }
        };
        server.onMessage = [](const std::shared_ptr<Channel> &incoming, hv::Buffer *buffer) noexcept
        {
            const auto channel = incoming;
            if (channel->http)
                channel->http->receive(static_cast<const char *>(buffer->data()), buffer->size());
        };
    }

    void report(NetworkError error) noexcept
    {
        try
        {
            if (errorHandler)
                errorHandler(error);
        } catch (...)
        {
        }
    }

    HttpServerOptions options;
    Handler handler;
    ErrorHandler errorHandler;
    HttpRequestErrorHandler requestErrorHandler;
    const std::uint32_t connectionLimit;
    std::atomic<std::uint64_t> connectionCount{0};
    std::unique_ptr<void, decltype(&hssl_ctx_free)> tls{nullptr, &hssl_ctx_free};
    hv::TcpServerTmpl<Channel> server; // Destroy/join loops before callbacks and TLS context.
};

HttpTransport::HttpTransport(HttpServerOptions options, std::uint32_t maxConnections, Handler handler, ErrorHandler onError,
                             HttpRequestErrorHandler onRequestError) :
    impl_(std::make_unique<Impl>(std::move(options), maxConnections, std::move(handler), std::move(onError),
                                 std::move(onRequestError)))
{
}
HttpTransport::~HttpTransport() = default;

auto HttpTransport::start() -> SZX::COMMON::Result<std::uint16_t>
{
    using Result        = SZX::COMMON::Result<std::uint16_t>;
    const auto &options = impl_->options;
    auto &server        = impl_->server;
    if (options.tls.enabled)
    {
        hssl_ctx_opt_t tls{};
        tls.crt_file    = options.tls.certFile.c_str();
        tls.key_file    = options.tls.keyFile.c_str();
        tls.ca_file     = options.tls.caFile.empty() ? nullptr : options.tls.caFile.c_str();
        tls.verify_peer = options.tls.verifyPeer;
        tls.endpoint    = HSSL_SERVER;
        impl_->tls.reset(hssl_ctx_new(&tls));
        if (!impl_->tls)
            return Result::Failure(static_cast<int>(NetworkError::TlsHandshakeFailed), {});
        server.withTLS();
    }
    const auto fd = server.createsocket(options.listen.port, options.listen.host.c_str());
    if (fd < 0)
        return Result::Failure(static_cast<int>(NetworkError::BindFailed), {});
    // Register ownership immediately, before any later failure can bypass cleanup.
    auto *listen = hio_get(server.loop()->loop(), fd);
    if (!listen)
    {
        ::closesocket(fd);
        server.listenfd = -1;
        return Result::Failure(static_cast<int>(NetworkError::Internal), {});
    }
    if (impl_->tls)
        hio_set_ssl_ctx(listen, impl_->tls.get());
    sockaddr_u address{};
    socklen_t length = sizeof(address);
    if (getsockname(fd, &address.sa, &length) != 0)
        return Result::Failure(static_cast<int>(NetworkError::BindFailed), {});
    const auto port = sockaddr_port(&address);
    server.start(true);
    return Result::Success(port);
}

void HttpTransport::stop()
{
    impl_->server.stop(true);
}
} // namespace SZX::NETWORK::v1::detail
