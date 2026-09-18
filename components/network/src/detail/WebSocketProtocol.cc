#include "WebSocketProtocol.h"

#include <array>
#include <hv/base64.h>
#include <random>

namespace SZX::NETWORK::v1::detail
{
namespace
{
auto equalAscii(std::string_view a, std::string_view b) noexcept -> bool
{
    const CaseInsensitiveLess less;
    return !less(a, b) && !less(b, a);
}
auto trim(std::string_view value) noexcept -> std::string_view
{
    const auto first = value.find_first_not_of(" \t");
    return first == value.npos ? std::string_view{} : value.substr(first, value.find_last_not_of(" \t") - first + 1);
}
auto hasToken(std::string_view value, std::string_view token) noexcept -> bool
{
    while (!value.empty())
    {
        const auto end = value.find(',');
        if (equalAscii(trim(value.substr(0, end)), token))
            return true;
        if (end == value.npos)
            break;
        value.remove_prefix(end + 1);
    }
    return false;
}
auto validKey(std::string_view key) noexcept -> bool
{
    // Canonical base64 for exactly 16 bytes, including the four zero padding bits.
    return key.size() == 24 && key.substr(22) == "==" &&
        key.substr(0, 22).find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/") == key.npos &&
        std::string_view("AQgw").find(key[21]) != key.npos;
}
} // namespace

WebSocketProtocol::WebSocketProtocol(hv::WebSocketChannel &channel, std::uint32_t maxMessage, std::uint32_t maxPending,
                                     Callbacks callbacks) :
    channel_(channel),
    maxMessage_(maxMessage),
    maxPending_(maxPending),
    callbacks_(std::move(callbacks))
{
    http_parser_init(&http_, channel.type == WS_SERVER ? HTTP_REQUEST : HTTP_RESPONSE);
    http_.data = this;
    websocket_parser_init(&frame_);
    frame_.data = this;
}

void WebSocketProtocol::startClient(HttpRequest &request) noexcept
{
    try
    {
        std::array<unsigned char, 16> nonce{};
        std::random_device random;
        for (auto &byte : nonce)
            byte = static_cast<unsigned char>(random());
        char key[32]{};
        hv_base64_encode(nonce.data(), static_cast<unsigned>(nonce.size()), key);
        char accept[32]{};
        ws_encode_key(key, accept);
        accept_                                = accept;
        request.headers["Connection"]          = "Upgrade";
        request.headers["Upgrade"]             = "websocket";
        request.headers[SEC_WEBSOCKET_KEY]     = key;
        request.headers[SEC_WEBSOCKET_VERSION] = "13";
        if (channel_.write(request.Dump(true, true)) < 0)
            fail(NetworkError::SendFailed);
    } catch (...)
    {
        fail(NetworkError::Internal);
    }
}

void WebSocketProtocol::receive(const char *data, std::size_t size) noexcept
{
    if (stopped_)
        return;
    if (!ready_)
    {
        const auto count = http_parser_execute(&http_, &httpSettings(), data, size);
        if (stopped_)
            return;
        if (HTTP_PARSER_ERRNO(&http_) != HPE_OK || (!ready_ && count != size))
        {
            if (channel_.type == WS_SERVER)
            {
                try
                {
                    reject(400);
                } catch (...)
                {
                    fail(NetworkError::Internal);
                }
            }
            else
                fail(NetworkError::ProtocolError);
            return;
        }
        data += count;
        size -= count;
    }
    if (ready_ && size && !stopped_)
    {
        const auto count = websocket_parser_execute(&frame_, &frameSettings(), data, size);
        if (!stopped_ && count != size)
            fail(NetworkError::ProtocolError);
    }
}

auto WebSocketProtocol::httpSettings() noexcept -> const http_parser_settings &
{
    static const auto settings = []
    {
        http_parser_settings value{};
        value.on_url = [](http_parser *p, const char *data, std::size_t size) noexcept
        {
            return callback(p, [&](auto &self) { self.target_.append(data, size); });
        };
        value.on_header_field = [](http_parser *p, const char *data, std::size_t size) noexcept
        {
            return callback(p,
                            [&](auto &self)
                            {
                                if (self.readingValue_)
                                    self.finishHeader();
                                self.field_.append(data, size);
                            });
        };
        value.on_header_value = [](http_parser *p, const char *data, std::size_t size) noexcept
        {
            return callback(p,
                            [&](auto &self)
                            {
                                self.readingValue_ = true;
                                self.value_.append(data, size);
                            });
        };
        value.on_headers_complete = [](http_parser *p) noexcept
        {
            // Stop hv's HTTP parser exactly at the upgrade boundary, retaining
            // any first WebSocket frame coalesced into this same TCP read.
            return callback(p, [](auto &self) { self.headersComplete(); }) == 0 ? 2 : -1;
        };
        value.on_message_complete = [](http_parser *p) noexcept
        {
            return callback(p, [](auto &self) { self.upgraded(); });
        };
        return value;
    }();
    return settings;
}

void WebSocketProtocol::finishHeader()
{
    if (const auto it = headers_.find(field_); it != headers_.end())
    {
        // Connection permits a token list. Singleton handshake fields must not
        // silently overwrite earlier values (including differently cased names).
        if (equalAscii(field_, "Host") || equalAscii(field_, "Upgrade") || equalAscii(field_, SEC_WEBSOCKET_KEY) ||
            equalAscii(field_, SEC_WEBSOCKET_VERSION) || equalAscii(field_, SEC_WEBSOCKET_ACCEPT) ||
            equalAscii(field_, "Content-Length") || equalAscii(field_, "Transfer-Encoding"))
        {
            if (channel_.type == WS_SERVER)
                reject(400);
            else
                fail(NetworkError::ProtocolError);
        }
        else
            it->second += "," + value_;
    }
    else
        headers_.emplace(field_, trim(value_));
    field_.clear();
    value_.clear();
    readingValue_ = false;
}

auto WebSocketProtocol::header(std::string_view name) const -> std::string_view
{
    const auto it = headers_.find(name);
    return it == headers_.end() ? std::string_view{} : it->second;
}

void WebSocketProtocol::headersComplete()
{
    finishHeader();
    if (stopped_)
        return;
    const bool valid = http_.http_major == 1 && http_.http_minor == 1 && equalAscii(header("Upgrade"), "websocket") &&
        hasToken(header("Connection"), "upgrade") && !headers_.contains("Transfer-Encoding") &&
        (!(http_.flags & F_CONTENTLENGTH) || http_.content_length == 0);
    if (channel_.type == WS_SERVER)
    {
        if (!headers_.contains("Upgrade") && !headers_.contains(SEC_WEBSOCKET_KEY) && !headers_.contains(SEC_WEBSOCKET_VERSION) &&
            !hasToken(header("Connection"), "upgrade"))
        {
            reject(404);
            return;
        }
        if (!valid || http_.method != HTTP_GET || header("Host").empty() || !target_.starts_with('/') ||
            target_.find('#') != target_.npos || header(SEC_WEBSOCKET_VERSION) != "13" || !validKey(header(SEC_WEBSOCKET_KEY)))
        {
            reject(400);
            return;
        }
        char accept[32]{};
        ws_encode_key(headers_.at(SEC_WEBSOCKET_KEY).c_str(), accept);
        accept_ = accept;
        target_.resize(target_.find('?') == target_.npos ? target_.size() : target_.find('?'));
    }
    else if (!valid || http_.status_code != 101 || header(SEC_WEBSOCKET_ACCEPT) != accept_ ||
             headers_.contains(SEC_WEBSOCKET_EXTENSIONS) || headers_.contains(SEC_WEBSOCKET_PROTOCOL))
        fail(NetworkError::ProtocolError);
}

void WebSocketProtocol::upgraded()
{
    if (channel_.type == WS_SERVER &&
        channel_.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
                       accept_ + "\r\n\r\n") < 0)
    {
        fail(NetworkError::SendFailed);
        return;
    }
    ready_ = true;
    headers_.clear();
    callbacks_.onOpen(target_);
}

auto WebSocketProtocol::frameSettings() noexcept -> const websocket_parser_settings &
{
    static const websocket_parser_settings settings{[](websocket_parser *p) noexcept
                                                    { return callback(p, [](auto &self) { self.frameHeader(); }); },
                                                    [](websocket_parser *p, const char *data, std::size_t size) noexcept
                                                    { return callback(p, [&](auto &self) { self.frameBody(data, size); }); },
                                                    [](websocket_parser *p) noexcept
                                                    {
                                                        return callback(p, [](auto &self) { self.frameEnd(); });
                                                    }};
    return settings;
}

void WebSocketProtocol::frameHeader()
{
    const auto opcode  = frame_.flags & WS_OP_MASK;
    const bool control = opcode >= WS_OP_CLOSE;
    if (((frame_.flags & WS_HAS_MASK) != 0) != (channel_.type == WS_SERVER) ||
        (control &&
         (!(frame_.flags & WS_FIN) || frame_.length > 125 ||
          (opcode != WS_OP_CLOSE && opcode != WS_OP_PING && opcode != WS_OP_PONG))) ||
        (!control && opcode != WS_OP_CONTINUE && opcode != WS_OP_TEXT && opcode != WS_OP_BINARY) ||
        (!control && ((opcode == WS_OP_CONTINUE) != fragmented_)))
    {
        fail(NetworkError::ProtocolError);
        return;
    }
    if (control)
        control_.clear();
    else
    {
        if (opcode != WS_OP_CONTINUE)
        {
            message_.clear();
            messageType_ = static_cast<ws_opcode>(opcode);
        }
        // Compare the full length BEFORE narrowing, reserving or reading a body.
        // Continuations count against the same message budget; ping/pong do not.
        if (frame_.length > maxMessage_ - message_.size())
            fail(NetworkError::MessageTooLarge);
    }
}

void WebSocketProtocol::frameBody(const char *data, std::size_t size)
{
    const bool control = (frame_.flags & WS_OP_MASK) >= WS_OP_CLOSE;
    auto &buffer       = control ? control_ : message_;
    if (size > (control ? 125 : maxMessage_) - buffer.size())
    {
        fail(NetworkError::MessageTooLarge);
        return;
    }
    const auto offset = buffer.size();
    buffer.resize(offset + size);
    if (frame_.flags & WS_HAS_MASK)
        websocket_parser_decode(buffer.data() + offset, data, size, &frame_);
    else
        buffer.replace(offset, size, data, size);
}

void WebSocketProtocol::frameEnd()
{
    const auto opcode = frame_.flags & WS_OP_MASK;
    if (opcode == WS_OP_PING)
        sendControl(control_, WS_OPCODE_PONG);
    else if (opcode == WS_OP_CLOSE)
    {
        if (control_.size() == 1)
            fail(NetworkError::ProtocolError);
        else
        {
            sendControl(control_, WS_OPCODE_CLOSE);
            stopped_ = true;
            channel_.SocketChannel::close(true);
        }
    }
    else if (opcode != WS_OP_PONG)
    {
        fragmented_ = !(frame_.flags & WS_FIN);
        if (!fragmented_)
        {
            channel_.opcode = messageType_;
            callbacks_.onMessage(message_);
            message_.clear();
        }
    }
}

void WebSocketProtocol::sendControl(const std::string &data, ws_opcode opcode)
{
    const auto cost = data.size() + 14;
    if (cost > maxPending_ || channel_.writeBufsize() > maxPending_ - cost)
        fail(NetworkError::SendQueueFull);
    else if (channel_.send(data, opcode) < 0)
        fail(NetworkError::SendFailed);
}

void WebSocketProtocol::reject(int status)
{
    channel_.write(status == 404 ? "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n" :
                                   "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    fail(NetworkError::ProtocolError);
}

void WebSocketProtocol::fail(NetworkError error) noexcept
{
    if (stopped_)
        return;
    stopped_ = true;
    error_   = error;
    try
    {
        if (ready_)
            callbacks_.onError(error);
    } catch (...)
    {
    }
    channel_.SocketChannel::close(true);
}
} // namespace SZX::NETWORK::v1::detail
