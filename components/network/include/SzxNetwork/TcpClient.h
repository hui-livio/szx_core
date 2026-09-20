#pragma once
#include <memory>
#include "Result.hpp"
#include "SzxNetwork/TcpTypes.h"
namespace SZX::NETWORK
{
inline namespace v1
{
struct TcpClientOptions final
{
    Endpoint remote;
    TcpFraming framing;
    std::uint32_t connectTimeoutMs = 5000; ///< 不保证限制系统 DNS 解析。
    std::uint32_t maxPendingBytes  = 4u * 1024u * 1024u;
};
/** @brief hv TCP 客户端薄封装；start 启动异步连接，onConnection 通知结果。
 * @details start 成功不表示已连接；失败经 onError 通知。不自动重连，stop 后可再次 start。
 * send 成功表示接收发送任务；分帧、线程及回调约束与 TcpServer 相同。
 */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class SZX_NETWORK_API TcpClient final
{
public:
    [[nodiscard]] static auto Create(TcpClientOptions options, std::shared_ptr<TcpClientCallbacks> callbacks = {})
        -> SZX::COMMON::Result<std::unique_ptr<TcpClient>>;
    ~TcpClient();
    TcpClient(const TcpClient &)                     = delete;
    auto operator=(const TcpClient &) -> TcpClient & = delete;
    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto send(std::string data) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto disconnect() -> SZX::COMMON::Result<void>;
    /** @brief Associates application-owned state with the active connection. */
    [[nodiscard]] auto setContext(TcpConnectionContext context) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto context() const -> SZX::COMMON::Result<TcpConnectionContext>;
    [[nodiscard]] auto isConnected() const noexcept -> bool
    {
        return connected();
    }
    [[nodiscard]] auto connected() const noexcept -> bool;

private:
    class Impl;
    explicit TcpClient(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace v1
} // namespace SZX::NETWORK
