#pragma once

#include <hv/HttpMessage.h>
#include <hv/WebSocketChannel.h>
#include "SzxNetwork/NetworkError.h"
#include "SzxNetwork/NetworkTypes.h"
#include "http_parser.h"
#include "websocket_parser.h"

namespace SZX::NETWORK::v1::detail
{
// Private adapter over hv's C parsers. The high-level hv WebSocketParser allocates
// before checking lengths and shares data/control state, so it cannot enforce our limits.
class WebSocketProtocol final
{
public:
    struct Callbacks
    {
        std::function<void(std::string_view)> onOpen;
        std::function<void(const std::string &)> onMessage;
        std::function<void(NetworkError)> onError;
    };
    WebSocketProtocol(hv::WebSocketChannel &channel, std::uint32_t maxMessage, std::uint32_t maxPending, Callbacks callbacks);
    void startClient(HttpRequest &request) noexcept;
    void receive(const char *data, std::size_t size) noexcept;
    auto ready() const noexcept -> bool
    {
        return ready_;
    }
    auto error() const noexcept -> NetworkError
    {
        return error_;
    }

private:
    template <typename Parser, typename Fn>
    static auto callback(Parser *parser, Fn &&fn) noexcept -> int
    {
        auto &self = *static_cast<WebSocketProtocol *>(parser->data);
        if (self.stopped_)
            return -1;
        try
        {
            fn(self);
        } catch (const std::bad_alloc &)
        {
            self.fail(NetworkError::ResourceExhausted);
        } catch (...)
        {
            self.fail(NetworkError::Internal);
        }
        return self.stopped_ ? -1 : 0;
    }
    static auto httpSettings() noexcept -> const http_parser_settings &;
    static auto frameSettings() noexcept -> const websocket_parser_settings &;
    void finishHeader();
    void headersComplete();
    void upgraded();
    void frameHeader();
    void frameBody(const char *data, std::size_t size);
    void frameEnd();
    void sendControl(const std::string &data, ws_opcode opcode);
    void reject(int status);
    void fail(NetworkError error) noexcept;
    auto header(std::string_view name) const -> std::string_view;

    hv::WebSocketChannel &channel_;
    const std::uint32_t maxMessage_, maxPending_;
    Callbacks callbacks_;
    http_parser http_{};
    websocket_parser frame_{};
    HttpHeaders headers_;
    std::string field_, value_, target_, accept_;
    std::string message_, control_;
    ws_opcode messageType_ = WS_OPCODE_TEXT;
    bool readingValue_{}, fragmented_{}, ready_{}, stopped_{};
    NetworkError error_ = NetworkError::ProtocolError;
};
} // namespace SZX::NETWORK::v1::detail
