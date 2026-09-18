#include "HttpClientTransport.h"
#include "Http1Parser.h"
#include "StringHelper.hpp"

#include <hv/HttpClient.h>
#include <hv/herr.h>
#include <hv/hsocket.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <mutex>

#if SZX_NETWORK_HAS_TLS
    #include <openssl/ssl.h>
    #include <openssl/x509v3.h>
#endif

namespace SZX::NETWORK::v1::detail
{
namespace
{
auto failed(NetworkError error) -> HttpClientResult
{
    return HttpClientResult::Failure(static_cast<int>(error), {});
}

auto encode(std::string_view input) -> std::string
{
    constexpr char hex[] = "0123456789ABCDEF";
    std::string output;
    for (const unsigned char c : input)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~')
            output.push_back(static_cast<char>(c));
        else
        {
            output.push_back('%');
            output.push_back(hex[c >> 4]);
            output.push_back(hex[c & 15]);
        }
    }
    return output;
}
auto parameters(const HttpParameters &params) -> std::string
{
    std::string output;
    for (const auto &[key, value] : params)
    {
        if (!output.empty())
            output += '&';
        output += encode(key) + '=' + encode(value);
    }
    return output;
}
auto token(std::string_view input) -> bool
{
    return !input.empty() &&
        std::all_of(input.begin(), input.end(),
                    [](unsigned char c)
                    {
                        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                            std::string_view{"!#$%&'*+-.^_`|~"}.find(c) != std::string_view::npos;
                    });
}
auto validHeaders(const HttpHeaders &headers) -> bool
{
    for (const auto &[name, value] : headers)
    {
        if (!token(name) ||
            std::any_of(value.begin(), value.end(), [](unsigned char c) { return (c < 32 && c != '\t') || c == 127; }))
            return false;
    }
    for (const auto name : {"Host", "Connection", "Content-Length", "Transfer-Encoding", "Expect", "Upgrade"})
        if (headers.contains(name))
            return false;
    return true;
}
auto transportError(int code, bool connecting, bool secure = false) -> NetworkError
{
    const auto value = code < 0 ? -code : code;
    if (value == ERR_TASK_TIMEOUT || value == ETIMEDOUT
#ifdef _WIN32
        || value == WSAETIMEDOUT || value == WSAEWOULDBLOCK
#endif
    )
        return connecting ? NetworkError::ConnectTimeout : NetworkError::ReadTimeout;
    if (value == ERR_SSL || value == ERR_SSL_HANDSHAKE || value == ERR_NEW_SSL || value == ERR_NEW_SSL_CTX)
        return NetworkError::TlsHandshakeFailed;
#ifdef _WIN32
    if (value == WSAHOST_NOT_FOUND || value == WSATRY_AGAIN || value == WSANO_DATA)
        return NetworkError::ResolveFailed;
#else
    if (code == EAI_NONAME || code == EAI_AGAIN || code == EAI_FAIL)
        return NetworkError::ResolveFailed;
#endif
    if (connecting && secure && value > 0 && value <= 10)
        return NetworkError::TlsHandshakeFailed;
    if (value == ERR_PARSE)
        return NetworkError::ProtocolError;
    if (connecting)
        return NetworkError::ConnectFailed;
    return NetworkError::SendFailed;
}

// Keep the bundled backend untouched. Use its socket/TLS and HTTP parser with
// per-request callbacks that distinguish informational from final responses.
auto exchange(http_client_t *client, int socket, ::HttpRequest &request, ::HttpResponse &response) -> int
{
    const auto deadline  = std::chrono::steady_clock::now() + std::chrono::seconds(request.timeout);
    const auto remaining = [&]()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    };
    const auto wire = request.Dump(true, true);
    for (std::size_t offset = 0; offset < wire.size();)
    {
        const auto left = remaining();
        if (left <= 0)
            return ERR_TASK_TIMEOUT;
        so_sndtimeo(socket, static_cast<int>(left));
        const auto count = http_client_send_data(client, wire.data() + offset,
                                                 static_cast<int>(std::min(wire.size() - offset, std::size_t{INT_MAX})));
        if (count <= 0)
        {
            const auto error = socket_errno();
            if (error == EINTR)
                continue;
            return remaining() <= 0 ? ERR_TASK_TIMEOUT : error ? error : ERR_SEND;
        }
        offset += count;
    }
    Http1Parser parser;
    parser.InitResponse(&response);
    auto settings                = Http1Parser::cbs;
    settings.on_headers_complete = [](http_parser *raw)
    {
        // HEAD is set before parsing; status-based exclusions apply to each message.
        if (raw->status_code / 100 == 1 || raw->status_code == 204 || raw->status_code == 304)
            raw->flags |= F_SKIPBODY;
        return Http1Parser::cbs.on_headers_complete(raw);
    };
    settings.on_message_complete = [](http_parser *raw)
    {
        const auto result = Http1Parser::cbs.on_message_complete(raw);
        if (raw->status_code / 100 == 1 && raw->status_code != 101)
            static_cast<Http1Parser *>(raw->data)->state = HP_START_REQ_OR_RES;
        return result;
    };
    if (request.method == HTTP_HEAD)
        parser.flags |= F_SKIPBODY;
    std::array<char, 4096> buffer{};
    while (!parser.IsComplete())
    {
        if (request.cancel)
            return ERR_PARSE;
        const auto left = remaining();
        if (left <= 0)
            return ERR_TASK_TIMEOUT;
        so_rcvtimeo(socket, static_cast<int>(left));
        const auto count = http_client_recv_data(client, buffer.data(), static_cast<int>(buffer.size()));
        if (count < 0)
        {
            const auto error = socket_errno();
            if (error == EINTR)
                continue;
            return remaining() <= 0 ? ERR_TASK_TIMEOUT : error ? error : ERR_RECV;
        }
        const auto consumed = http_parser_execute(&parser.parser, &settings, buffer.data(), count);
        if (HTTP_PARSER_ERRNO(&parser.parser) != HPE_OK || consumed != static_cast<std::size_t>(count))
            return ERR_PARSE;
        if (count == 0 && !parser.IsComplete())
            return ERR_PARSE;
    }
    return 0;
}
} // namespace

auto sendHttpRequest(std::string method, std::string url, HttpRequestOptions options) -> HttpClientResult
{
    try
    {
        constexpr std::array<std::string_view, 8> methods{"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS", "TRACE"};
        if (std::find(methods.begin(), methods.end(), method) == methods.end() || url.size() > 65536 ||
            options.timeoutSeconds == 0 || options.connectTimeoutSeconds == 0 || options.maxResponseBytes == 0 ||
            !validHeaders(options.headers) ||
            (options.data.has_value() + options.json.has_value() + options.form.has_value()) > 1)
            return failed(NetworkError::InvalidArgument);
        if (!url.starts_with("http://") && !url.starts_with("https://"))
            return failed(NetworkError::InvalidArgument);
        if (std::any_of(url.begin(), url.end(), [](unsigned char c) { return c <= 32 || c >= 127 || c == '\\'; }))
            return failed(NetworkError::InvalidArgument);
        if (const auto fragment = url.find('#'); fragment != std::string::npos)
            url.resize(fragment);
        const bool secure       = url.starts_with("https://");
        const auto authorityEnd = url.find_first_of("/?", secure ? 8 : 7);
        if (authorityEnd == std::string::npos || url[authorityEnd] == '?')
            url.insert(authorityEnd == std::string::npos ? url.size() : authorityEnd, "/");
        hurl_t parsed{};
        if (hv_parse_url(&parsed, url.c_str()) != 0 || parsed.fields[HV_URL_HOST].len == 0)
            return failed(NetworkError::InvalidArgument);
        const auto host      = url.substr(parsed.fields[HV_URL_HOST].off, parsed.fields[HV_URL_HOST].len);
        const auto authority = url.substr(secure ? 8 : 7, url.find('/', secure ? 8 : 7) - (secure ? 8 : 7));
        if (authority.find('@') != std::string::npos || host.find_first_of("[]%?#") != std::string::npos)
            return failed(NetworkError::InvalidArgument);
        int port = secure ? 443 : 80;
        if (parsed.fields[HV_URL_PORT].len)
        {
            const auto value = url.substr(parsed.fields[HV_URL_PORT].off, parsed.fields[HV_URL_PORT].len);
            if (!std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; }))
                return failed(NetworkError::InvalidArgument);
            const auto number = SZX::COMMON::StringHelper::ParseInteger<unsigned>(value);
            if (!number || *number == 0 || *number > 65535)
                return failed(NetworkError::InvalidArgument);
            port = static_cast<int>(*number);
        }
        const auto expectedAuthority = (host.find(':') == std::string::npos ? host : '[' + host + ']') +
            (parsed.fields[HV_URL_PORT].len ? ':' + url.substr(parsed.fields[HV_URL_PORT].off, parsed.fields[HV_URL_PORT].len) :
                                              "");
        if (authority != expectedAuthority)
            return failed(NetworkError::InvalidArgument);
        const auto query = parameters(options.params);
        if (!query.empty())
            url += (url.find('?') == std::string::npos ? "?" : url.ends_with('?') || url.ends_with('&') ? "" : "&") + query;
        if (url.size() > 65536)
            return failed(NetworkError::InvalidArgument);
#if !SZX_NETWORK_HAS_TLS
        if (secure)
            return failed(NetworkError::TlsUnavailable);
#endif
        ::HttpRequest request;
        request.SetMethod(method.c_str());
        request.url             = url;
        request.timeout         = options.timeoutSeconds;
        request.connect_timeout = options.connectTimeoutSeconds;
        request.retry_count     = 0;
        request.redirect        = false;
        request.headers.clear();
        for (const auto &[name, value] : options.headers)
            request.headers[name] = value;
        request.headers["Connection"] = "close";
        request.headers["Host"]       = host.find(':') == std::string::npos ? host : '[' + host + ']';
        if (port != (secure ? 443 : 80))
            request.headers["Host"] += ':' + std::to_string(port);
        if (options.json)
        {
            request.body = options.json->dump();
            request.headers.try_emplace("Content-Type", "application/json");
        }
        else if (options.form)
        {
            request.body = parameters(*options.form);
            request.headers.try_emplace("Content-Type", "application/x-www-form-urlencoded");
        }
        else if (options.data)
            request.body = std::move(*options.data);
        if (request.body.size() > static_cast<std::size_t>(INT_MAX) - 65536)
            return failed(NetworkError::MessageTooLarge);

        std::unique_ptr<void, decltype(&hssl_ctx_free)> tls(nullptr, &hssl_ctx_free);
        std::unique_ptr<http_client_t, decltype(&http_client_del)> client(http_client_new(host.c_str(), port, secure),
                                                                          &http_client_del);
        if (!client)
            return failed(NetworkError::ResourceExhausted);
#if SZX_NETWORK_HAS_TLS
        if (secure)
        {
            hssl_ctx_opt_t config{};
            config.endpoint    = HSSL_CLIENT;
            config.verify_peer = options.verifyPeer;
            config.ca_file     = options.caFile.empty() ? nullptr : options.caFile.c_str();
            // The bundled backend's initialization flag is not atomic.
            static std::mutex tlsInitialization;
            {
                std::lock_guard lock(tlsInitialization);
                tls.reset(hssl_ctx_new(&config));
            }
            if (!tls)
                return failed(NetworkError::TlsHandshakeFailed);
            if (options.verifyPeer)
            {
                auto *verify      = SSL_CTX_get0_param(static_cast<SSL_CTX *>(tls.get()));
                const int checked = is_ipaddr(host.c_str()) ? X509_VERIFY_PARAM_set1_ip_asc(verify, host.c_str()) :
                                                              X509_VERIFY_PARAM_set1_host(verify, host.c_str(), host.size());
                if (checked != 1)
                    return failed(NetworkError::TlsHandshakeFailed);
                X509_VERIFY_PARAM_set_hostflags(verify, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            }
            http_client_set_ssl_ctx(client.get(), tls.get());
        }
#endif
        const auto connected = http_client_connect(client.get(), host.c_str(), port, secure, options.connectTimeoutSeconds);
        if (connected < 0)
            return failed(transportError(connected, true, secure));
        HttpClientResponse output;
        output.url = url;
        ::HttpResponse response;
        std::optional<NetworkError> callbackError;
        bool complete = false;
        // Stream body chunks so the backend cannot accumulate an unbounded body.
        response.http_cb = [&](HttpMessage *message, http_parser_state state, const char *data, std::size_t size) noexcept
        {
            try
            {
                if (state == HP_MESSAGE_BEGIN)
                {
                    message->Reset();
                    complete = false;
                    output.body.clear();
                }
                if (state == HP_HEADERS_COMPLETE)
                {
                    output.headers.clear();
                    for (const auto &[name, value] : message->headers)
                        output.headers[name] = value;
                    const auto status  = static_cast<::HttpResponse *>(message)->status_code;
                    const bool hasBody = method != "HEAD" && status >= 200 && status != 204 && status != 304;
                    if (hasBody && message->content_length > options.maxResponseBytes)
                        callbackError = NetworkError::MessageTooLarge;
                }
                if (state == HP_BODY && !callbackError)
                {
                    if (size > options.maxResponseBytes - output.body.size())
                        callbackError = NetworkError::MessageTooLarge;
                    else
                        output.body.append(data, size);
                }
                if (state == HP_MESSAGE_COMPLETE)
                    complete = static_cast<::HttpResponse *>(message)->status_code >= 200;
            } catch (const std::bad_alloc &)
            {
                callbackError = NetworkError::ResourceExhausted;
            } catch (...)
            {
                callbackError = NetworkError::Internal;
            }
            if (callbackError)
                request.cancel = true;
        };
        const auto sent = exchange(client.get(), connected, request, response);
        if (callbackError)
            return failed(*callbackError);
        if (sent != 0)
            return failed(transportError(sent, false));
        if (!complete || response.status_code < 200 || response.status_code > 599)
            return failed(NetworkError::ProtocolError);
        output.statusCode = response.status_code;
        for (auto &cookie : response.cookies)
            output.setCookies.push_back(cookie.dump());
        return HttpClientResult::Success(std::move(output));
    } catch (const std::bad_alloc &)
    {
        return failed(NetworkError::ResourceExhausted);
    } catch (const SZX::COMMON::Json::exception &)
    {
        return failed(NetworkError::InvalidArgument);
    } catch (...)
    {
        return failed(NetworkError::Internal);
    }
}
} // namespace SZX::NETWORK::v1::detail
