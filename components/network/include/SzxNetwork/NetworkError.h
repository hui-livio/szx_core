#pragma once
/** @file NetworkError.h
 * @brief 稳定的网络模块错误码；保留值与网络库设计保持一致。
 */
#include <functional>
#include <string>

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief 存储在 COMMON::Result::error 中的模块错误码，禁止重编号。 */
enum class NetworkError : int
{
    InvalidArgument       = 1,  ///< 配置、路由语法、回调或静态路径非法。
    NotInitialized        = 2,  ///< 保留：操作要求端点处于运行状态。
    AlreadyRunning        = 3,  ///< 运行中尝试启动或修改配置。
    ResolveFailed         = 4,  ///< 保留：DNS 解析失败。
    ConnectFailed         = 5,  ///< 保留：客户端连接失败。
    ConnectTimeout        = 6,  ///< 保留：客户端连接超时。
    BindFailed            = 7,  ///< 无法创建或绑定监听套接字。
    Closed                = 8,  ///< 保留：连接已关闭。
    SendFailed            = 9,  ///< 传输层发送失败。
    SendQueueFull         = 10, ///< 保留：发送队列已满。
    ReadTimeout           = 11, ///< 保留：读取超时。
    MessageTooLarge       = 12, ///< 请求体超过配置上限，对应 HTTP 413。
    ProtocolError         = 13, ///< 请求解析错误，或处理回调返回非法响应（HTTP 500）。
    RouteConflict         = 14, ///< 同方法、同路径形状或静态目录前缀重复。
    TlsUnavailable        = 15, ///< 当前构建未启用 TLS，但请求使用 TLS。
    TlsHandshakeFailed    = 16, ///< TLS 上下文或证书初始化失败。
    Cancelled             = 17, ///< 保留：异步操作已取消。
    Internal              = 18, ///< 未预期异常，或在回调内执行可能阻塞的生命周期操作。
    ResourceExhausted     = 19, ///< 内存或任务资源分配失败。
    HttpClientUnavailable = 20  ///< 构建时关闭了 HTTP 客户端。
};
/** @brief 在请求所属 IO 线程调用；诊断消息经过清理，不传递原始异常文本。 */
using ErrorCallback = std::function<void(NetworkError, const std::string &)>;
} // namespace v1
} // namespace SZX::NETWORK
