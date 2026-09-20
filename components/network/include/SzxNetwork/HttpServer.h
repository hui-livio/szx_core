#pragma once
/** @file HttpServer.h
 * @brief 同步 HTTP/1.1 服务端门面，公共 API 版本为 v1。
 */
#include <cstddef>
#include <filesystem>
#include <memory>
#include "Result.hpp"
#include "SzxNetwork/NetworkError.h"
#include "SzxNetwork/NetworkTypes.h"

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief 请求拥有其字段数据，在同步中间件和处理回调调用期间有效。 */
struct HttpServerRequest final
{
    std::string method;                            ///< 大写 HTTP 方法名。
    std::string path;                              ///< 不含查询字符串的 URL 路径，保留百分号编码。
    std::string body;                              ///< 支持二进制内容，长度受 maxBodyBytes 限制。
    HttpHeaders headers;                           ///< 头字段名按 ASCII 大小写不敏感比较。
    std::map<std::string, std::string> query;      ///< 已解码的查询参数，参数名区分大小写。
    std::map<std::string, std::string> pathParams; ///< 已解码的 {name} 路径参数，与查询参数独立。
    Endpoint peer;                                 ///< 发起请求的对端地址。
    std::map<std::string, std::string> attributes; ///< 初始为空，由应用中间件写入。
    std::shared_ptr<void> userContext;             ///< 初始为空，由应用管理的请求上下文。
};
/** @brief HTTP 响应；非法状态码或头字段将转换为 HTTP 500。 */
struct HttpServerResponse final
{
    int statusCode = 200; ///< 最终状态码，范围为 200..599；不支持信息响应或协议升级响应。
    std::string body;     ///< 响应体，支持二进制内容；HEAD 请求不在网络上传输此字段。
    HttpHeaders headers;  ///< Content-Length 和传输定界由服务端管理。
};
/** @brief 同步路由处理回调，根据请求返回响应。 */
using HttpRouteHandler = std::function<HttpServerResponse(const HttpServerRequest &)>;
/** @brief 中间件的后续调用入口，不得保存到异步任务中。 */
using HttpNext = std::function<HttpServerResponse(HttpServerRequest &)>;
/** @brief 先注册的中间件位于最外层；next 必须同步调用，且最多调用一次。 */
using HttpMiddleware = std::function<HttpServerResponse(HttpServerRequest &, const HttpNext &)>;
/** @brief 自定义错误响应；请求可能不完整，原始 HTTP 错误状态码不可更改。 */
using HttpRequestErrorHandler = std::function<HttpServerResponse(const HttpServerRequest &, int, NetworkError)>;

/** @brief HTTP 服务端配置。每个实例独立拥有 IO 线程，不共享事件循环。 */
struct HttpServerOptions final
{
    Endpoint listen;                                       ///< 监听地址和端口。
    std::size_t ioThreadCount        = 4;                  ///< IO 线程总数，范围为 1..64。
    std::size_t maxBodyBytes         = 1u * 1024u * 1024u; ///< 非零请求体上限；定长和分块报文超限时提前返回 413。
    std::uint32_t keepaliveTimeoutMs = 60000;              ///< 连接保活超时，单位为毫秒；非零且不超过 INT_MAX。
    TlsOptions tls;                                        ///< TLS 配置，默认禁用。
};

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
/**
 * @brief 独立管理生命周期的 HTTP 服务端，不内置认证或授权策略。
 * @details 只能在停止状态修改配置；调用方须串行执行配置和生命周期操作。
 * running()/port() 支持并发读取。回调在 IO 线程执行，不同连接的回调可能并发；
 * 不得在回调内执行阻塞操作或析构服务端。回调捕获的对象必须存活到 stop() 返回之后。
 * 处理回调或中间件抛出异常时返回 500 并通知 onError(Internal)；错误观察器的异常会被拦截。
 * DLL 与调用方必须使用匹配的编译器、STL 和运行时配置。
 */
class SZX_NETWORK_API HttpServer final
{
public:
    /** @brief 校验配置并创建服务端，不打开套接字。空监听地址默认使用 127.0.0.1。
     * @param[in] options 服务端配置。
     * @return 成功返回服务端的独占所有权；失败返回 InvalidArgument、TlsUnavailable 或 Internal。
     */
    [[nodiscard]] static auto Create(HttpServerOptions options) -> SZX::COMMON::Result<std::unique_ptr<HttpServer>>;
    /** @brief 创建可复用的 GET/HEAD 文件处理器，将文件访问限制在指定目录内。
     * @param[in] directory 已存在的根目录；服务期间不得允许不可信进程修改目录树。
     * @param[in] homePage 目录首页的相对路径，必须位于 directory 内。
     * @return 成功返回独立拥有配置的处理器；失败返回 InvalidArgument 或 Internal。文件不存在时返回 404，
     * 路径不安全时返回 403，文件超过 16 MiB 时返回 413，读取失败或异常时返回 500。
     * @note 请求路径相对 URL 根目录解析，不自动执行 SPA 回退；处理器支持并发调用。
     */
    [[nodiscard]] static auto CreateStaticHandler(std::filesystem::path directory, std::string homePage = "index.html")
        -> SZX::COMMON::Result<HttpRouteHandler>;
    /** @brief 停止服务并等待所有工作线程退出；必须在所有网络回调之外执行。 */
    ~HttpServer();
    HttpServer(const HttpServer &)                     = delete;
    auto operator=(const HttpServer &) -> HttpServer & = delete;

    /** @brief 注册 GET/HEAD/POST/PUT/PATCH/DELETE/OPTIONS/TRACE 路由，方法名必须大写。
     * @param[in] method HTTP 方法名。
     * @param[in] path 绝对路径；支持完整路径段 {name} 参数或末尾 /* 通配符。
     * @param[in] handler 同步请求处理回调。
     * @return 语法非法时返回 InvalidArgument；同方法、同路径形状重复注册时返回 RouteConflict。
     * @note 字面量路径段优先于参数，参数优先于通配符；匹配程度相同时按注册顺序选择。
     * HEAD 可回退到 GET，并禁止发送响应体；路径匹配但方法不匹配时返回 405。
     */
    [[nodiscard]] auto route(std::string method, std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief 注册 GET 路由。 @see route */
    [[nodiscard]] auto get(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief 注册 POST 路由。 @see route */
    [[nodiscard]] auto post(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief 注册 PUT 路由。 @see route */
    [[nodiscard]] auto put(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief 注册 DELETE 路由。 @see route */
    [[nodiscard]] auto del(std::string path, HttpRouteHandler handler) -> SZX::COMMON::Result<void>;

    /** @brief 挂载 GET/HEAD 静态目录；显式路由优先，静态响应经过中间件。
     * @param[in] urlPrefix 不含参数或通配符的绝对 URL 前缀；优先选择最长匹配前缀。
     * @param[in] directory 已存在的目录；服务期间不得允许不可信进程修改目录树。
     * @param[in] fallbackFile 可选回退文件，须已存在且使用相对路径，仅对本挂载范围内的缺失文件生效。
     * @return 路径非法或越界时返回 InvalidArgument；前缀重复时返回 RouteConflict。
     * @note 拒绝目录穿越、盘符/数据流路径及符号链接逃逸。文件大小上限为 16 MiB；
     * 超限返回 413。目录请求使用 index.html，不提供目录列表。
     */
    [[nodiscard]] auto mountStatic(std::string urlPrefix, std::filesystem::path directory, std::string fallbackFile = {})
        -> SZX::COMMON::Result<void>;
    /** @brief 追加路由选定后的洋葱中间件。
     * @param[in] middleware 同步中间件；进入回调时已完成路由匹配，可读取 pathParams。
     * @return 成功结果，或 InvalidArgument、AlreadyRunning、Internal 错误。
     */
    [[nodiscard]] auto use(HttpMiddleware middleware) -> SZX::COMMON::Result<void>;
    /** @brief 追加路由匹配前的中间件，用于改写请求或提前拒绝。
     * @param[in] middleware 同步洋葱中间件，可在调用 next() 前修改 path/method。
     * @return 成功返回成功结果；失败返回 InvalidArgument、AlreadyRunning 或 Internal。
     * @note 先注册的中间件先执行。此链结束后，使用处理后的请求匹配路由，
     * 再执行 use() 中间件；pathParams 仅在路由匹配后填充。
     */
    [[nodiscard]] auto useBeforeRouting(HttpMiddleware middleware) -> SZX::COMMON::Result<void>;
    /** @brief 在停止状态替换默认的空 404 响应处理器。
     * @param[in] handler 非空的同步未命中处理回调。
     * @return 成功结果，或 InvalidArgument、AlreadyRunning、Internal 错误。
     */
    [[nodiscard]] auto onNotFound(HttpRouteHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief 在停止状态设置错误观察器；传入空回调可关闭通知。
     * @param[in] callback 错误观察回调。
     * @return 成功结果，或 AlreadyRunning、Internal 错误。
     * @note 接收回调异常、协议错误和请求体超限通知；同步 API 失败通过返回值直接报告。
     */
    [[nodiscard]] auto onError(ErrorCallback callback) -> SZX::COMMON::Result<void>;
    /** @brief 自定义解析失败、请求体超限及处理回调/中间件未捕获异常的错误响应。
     * @param[in] handler 错误响应回调；传入空回调可恢复默认空错误响应。
     * @return 成功返回成功结果；失败返回 AlreadyRunning 或 Internal。
     * @note 在中间件链之外执行，遵循相同的回调生命周期约定。部分请求数据仍须视为不可信；
     * 原始 HTTP 状态码及连接定界由服务端保留。回调抛异常或
     * 返回非法响应头时退回空错误响应；onError 仍会收到原始错误通知。
     */
    [[nodiscard]] auto onRequestError(HttpRequestErrorHandler handler) -> SZX::COMMON::Result<void>;
    /** @brief 在停止状态设置整个服务端允许的连接总数上限。
     * @param[in] count 非零连接上限；超限连接在处理请求前关闭。
     * @return 成功返回成功结果；失败返回 InvalidArgument、AlreadyRunning 或 Internal。
     * @note 默认值为 ioThreadCount * 1024；计数包含空闲保活连接。
     */
    [[nodiscard]] auto setMaxConnections(std::uint32_t count) -> SZX::COMMON::Result<void>;
    /** @brief 绑定监听地址并启动 IO 线程；绑定失败或重复启动时返回 BindFailed 或 AlreadyRunning。 */
    [[nodiscard]] auto start() -> SZX::COMMON::Result<void>;
    /** @brief 关闭连接，停止并等待 IO 线程退出；支持幂等调用，重启保留已有配置。
     * @return 成功后保证不再调用应用回调；在任意网络回调内调用时返回 Internal。
     * @note 会等待已进入的处理回调完成，但不保证待发送响应交付成功。
     */
    [[nodiscard]] auto stop() -> SZX::COMMON::Result<void>;
    /** @brief 返回服务是否已成功启动且尚未完成停止。 */
    [[nodiscard]] auto running() const noexcept -> bool;
    /** @brief 返回运行时实际绑定的端口；停止状态返回 0。 */
    [[nodiscard]] auto port() const noexcept -> std::uint16_t;

private:
    class Impl;
    explicit HttpServer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
} // namespace v1
} // namespace SZX::NETWORK
