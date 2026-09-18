#pragma once
#include <memory>
#include "Result.hpp"
#include "SzxNetwork/TcpTypes.h"
namespace SZX::NETWORK
{
inline namespace v1
{
struct TcpServerOptions final
{
    Endpoint listen; ///< 空地址默认回环地址，端口 0 由系统分配。
    TcpFraming framing;
    std::uint32_t maxConnections  = 1024;
    std::uint32_t maxPendingBytes = 4u * 1024u * 1024u; ///< 每连接发送队列上限。
};
/** @brief hv TCP 服务端薄封装，每实例一个 IO 线程，无额外任务线程池。
 * @details 负责监听、连接查询、计数与遍历；单连接 IO 和上下文由 TcpHandle 负责。
 * 生命周期由调用方串行控制；broadcast 为遍历连接并调用 TcpHandle::send 的批量操作。
 * 分帧仅作用于接收，不提供 TLS/自动重连。失败 Result.message 为空。
 * 公共 API v1；DLL 调用方须使用兼容编译器/STL/运行时。
 */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class SZX_NETWORK_API TcpServer final
{
public:
    [[nodiscard]] static auto Create(TcpServerOptions options = {}, std::shared_ptr<TcpServerCallbacks> callbacks = {})
        -> SZX::COMMON::Result<std::unique_ptr<TcpServer>>;
    ~TcpServer();
    TcpServer(const TcpServer &)                     = delete;
    auto operator=(const TcpServer &) -> TcpServer & = delete;
    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;
    /** @brief Sends to active peers; stops at first failure without undoing previous submissions. */
    [[nodiscard]] auto broadcast(std::string data) -> SZX::COMMON::Result<void>;
    /** @brief Visits handles on the caller thread without locks; visitor may send/disconnect but must not start/stop/destroy. */
    [[nodiscard]] auto forEach(TcpChannelVisitor &visitor) -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto foreachChannel(TcpChannelVisitor &visitor) -> SZX::COMMON::Result<void>
    {
        return forEach(visitor);
    }
    [[nodiscard]] auto getChannelById(TcpConnectionId id) const -> SZX::COMMON::Result<TcpHandle>;
    [[nodiscard]] auto connectionNum() const -> SZX::COMMON::Result<std::size_t>;
    [[nodiscard]] auto running() const noexcept -> bool;
    [[nodiscard]] auto port() const noexcept -> std::uint16_t;

private:
    class Impl;
    explicit TcpServer(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace v1
} // namespace SZX::NETWORK
