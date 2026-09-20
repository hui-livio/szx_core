#pragma once
#include <memory>
#include "Result.hpp"
#include "SzxNetwork/NetworkError.h"
#include "SzxNetwork/NetworkTypes.h"
namespace SZX::NETWORK
{
inline namespace v1
{
using TcpConnectionId      = std::uint64_t;
using TcpConnectionContext = std::shared_ptr<void>;
struct TcpState;
/** @brief Copyable connection handle. Retaining it does not keep the endpoint alive.
 * Operations on an expired/closed connection return Closed. Context contents require application synchronization.
 */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class SZX_NETWORK_API TcpHandle final
{
public:
    TcpHandle() = default;
    [[nodiscard]] auto id() const noexcept -> TcpConnectionId
    {
        return id_;
    }
    /** @brief Snapshot only; the connection may close immediately after this call. */
    [[nodiscard]] auto isConnected() const noexcept -> bool;
    [[nodiscard]] auto peeraddr() const -> SZX::COMMON::Result<Endpoint>;
    [[nodiscard]] auto write(std::string data) const -> SZX::COMMON::Result<void>
    {
        return send(std::move(data));
    }
    [[nodiscard]] auto close() const -> SZX::COMMON::Result<void>
    {
        return disconnect();
    }
    [[nodiscard]] auto setContextPtr(TcpConnectionContext value) const -> SZX::COMMON::Result<void>
    {
        return setContext(std::move(value));
    }
    [[nodiscard]] auto contextPtr() const -> SZX::COMMON::Result<TcpConnectionContext>
    {
        return context();
    }
    [[nodiscard]] auto deleteContextPtr() const -> SZX::COMMON::Result<void>
    {
        return setContext({});
    }
    /** @brief T must match the stored context type; an unset context succeeds with nullptr. */
    template <typename T>
    [[nodiscard]] auto getContextPtr() const -> SZX::COMMON::Result<std::shared_ptr<T>>
    {
        auto value = context();
        if (!value.ok)
            return SZX::COMMON::Result<std::shared_ptr<T>>::Failure(value.error, {});
        return SZX::COMMON::Result<std::shared_ptr<T>>::Success(std::static_pointer_cast<T>(value.value));
    }
    /** @brief Constructs and replaces shared context, forwarding constructor arguments. */
    template <typename T, typename... Args>
    [[nodiscard]] auto newContextPtr(Args &&...args) const -> SZX::COMMON::Result<std::shared_ptr<T>>
    {
        using Result = SZX::COMMON::Result<std::shared_ptr<T>>;
        try
        {
            auto value = std::make_shared<T>(std::forward<Args>(args)...);
            auto saved = setContext(value);
            if (!saved.ok)
                return Result::Failure(saved.error, {});
            return Result::Success(std::move(value));
        } catch (const std::bad_alloc &)
        {
            return Result::Failure(static_cast<int>(NetworkError::ResourceExhausted), {});
        } catch (...)
        {
            return Result::Failure(static_cast<int>(NetworkError::Internal), {});
        }
    }
    [[nodiscard]] auto send(std::string data) const -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto disconnect() const -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto setContext(TcpConnectionContext context) const -> SZX::COMMON::Result<void>;
    [[nodiscard]] auto context() const -> SZX::COMMON::Result<TcpConnectionContext>;

private:
    friend struct TcpState;
    TcpHandle(std::weak_ptr<TcpState> state, TcpConnectionId id) :
        state_(std::move(state)),
        id_(id)
    {
    }
    std::weak_ptr<TcpState> state_;
    TcpConnectionId id_{};
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
enum class TcpFrameMode
{
    Raw,
    FixedLength,
    Delimiter,
    LengthField
};
/** @brief 映射到 hv 内置 unpack；接收保留完整帧（含头/分隔符），发送不自动组帧。
 * @details 长度字段采用无符号大/小端整数，支持 1..3 字节；不暴露有溢出风险的 4 字节/varint 配置。
 * 帧长度 = bodyOffset + 长度字段值，长度字段表示正文长度。
 */
struct TcpFraming final
{
    TcpFrameMode mode               = TcpFrameMode::Raw;
    std::uint32_t maxFrameBytes     = 1024u * 1024u;
    std::uint32_t fixedLength       = 0;
    std::string delimiter           = "\n"; ///< 1..8 字节，支持二进制分隔符。
    std::uint16_t bodyOffset        = 2;
    std::uint16_t lengthFieldOffset = 0;
    std::uint16_t lengthFieldBytes  = 2;
    bool littleEndian               = false;
};
/** @brief 回调在 hv IO 线程执行，数据 view 仅在回调期间有效。
 * @details 回调内可以 send/disconnect；不得阻塞、start/stop 或析构端点。
 * 回调异常被拦截，报告 Internal 并关闭连接；错误观察器异常被忽略。
 * stop 返回后不再回调，主动 stop 不保证逐连接通知；析构不得与公共调用并发。
 * Create 通过 shared_ptr 持有独立回调对象至端点销毁；业务须实现全部纯虚方法。
 */
struct TcpServerCallbacks
{
    virtual ~TcpServerCallbacks()                               = default;
    virtual void onConnection(const TcpHandle &)                = 0;
    virtual void onMessage(const TcpHandle &, std::string_view) = 0;
    virtual void onError(const TcpHandle &, NetworkError)       = 0;
};
/** @brief Single-connection client events; operate through the owning TcpClient. */
struct TcpClientCallbacks
{
    virtual ~TcpClientCallbacks()                  = default;
    virtual void onConnection(bool connected)      = 0;
    virtual void onMessage(std::string_view bytes) = 0;
    virtual void onError(NetworkError error)       = 0;
};
struct TcpChannelVisitor
{
    virtual ~TcpChannelVisitor()              = default;
    virtual void onChannel(const TcpHandle &) = 0;
};
} // namespace v1
} // namespace SZX::NETWORK
