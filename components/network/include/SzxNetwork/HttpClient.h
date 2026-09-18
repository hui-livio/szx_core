#pragma once
/** @file HttpClient.h
 * @brief requests 风格的 HTTP/1.1 同步及 future 客户端，公共 API v1。
 */
#include <future>
#include <memory>
#include <optional>
#include <vector>
#include "Result.hpp"
#include "Serializer.hpp"
#include "SzxNetwork/NetworkError.h"
#include "SzxNetwork/NetworkTypes.h"

namespace SZX::NETWORK
{
inline namespace v1
{
/** @brief 保留顺序及重复键的查询/表单参数；自动按 UTF-8 字节进行百分号编码。 */
using HttpParameters = std::vector<std::pair<std::string, std::string>>;

/** @brief 每次请求独立拥有选项；data、json、form 至多指定一个。 */
struct HttpRequestOptions final
{
    HttpHeaders headers;                      ///< 请求头，禁止覆盖 Host/Connection/Content-Length/Transfer-Encoding/Expect。
    HttpParameters params;                    ///< 追加到 URL 现有查询串，不覆盖重复键。
    std::optional<std::string> data;          ///< 原始二进制正文。
    std::optional<SZX::COMMON::Json> json;    ///< JSON 正文，自动设置 Content-Type（可显式覆盖）。
    std::optional<HttpParameters> form;       ///< application/x-www-form-urlencoded 正文。
    std::uint16_t timeoutSeconds        = 30; ///< 非零发送/接收期限；不含排队和连接阶段。
    std::uint16_t connectTimeoutSeconds = 5;  ///< 非零连接期限；系统 DNS 解析不保证受此期限限制。
    std::size_t maxResponseBytes        = 16u * 1024u * 1024u; ///< 非零响应体上限。
    bool verifyPeer                     = true;                ///< HTTPS 校验证书链及主机名/IP；仅 TLS 构建可用。
    std::string caFile;                                        ///< 可选 PEM 信任库；为空使用 TLS 后端默认信任库。
};

/** @brief 收到 HTTP 4xx/5xx 仍属于传输成功；通过 statusCode/ok() 检查业务状态。 */
struct HttpClientResponse final
{
    int statusCode = 0;
    std::string body; ///< 原始响应字节；不自动解压或转码。
    HttpHeaders headers;
    std::vector<std::string> setCookies; ///< 各 Set-Cookie 的后端规范化值，不自动保存或发送 Cookie。
    std::string url;                     ///< 实际请求 URL，含编码后的 params，不含 fragment。
    [[nodiscard]] SZX_NETWORK_API auto ok() const noexcept -> bool;
    [[nodiscard]] SZX_NETWORK_API auto text() const noexcept -> const std::string &;
    /** @brief 显式解析 JSON；非法 JSON 返回 ProtocolError，不抛解析异常。 */
    [[nodiscard]] SZX_NETWORK_API auto json() const -> SZX::COMMON::Result<SZX::COMMON::Json>;
};
using HttpClientResult = SZX::COMMON::Result<HttpClientResponse>;
using HttpClientFuture = std::future<HttpClientResult>;

struct HttpClientOptions final
{
    std::size_t workerCount        = 4;  ///< 异步 worker 数量，1..64。
    std::size_t maxPendingRequests = 64; ///< 已接受的异步请求总上限，包含运行中和排队中的任务。
};

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
/** @brief 可复用且支持并发请求的客户端。
 * @details 同步请求在调用线程运行；异步任务使用独立有界线程池，future 的析构不会等待请求。
 * 析构客户端等待所有已接受任务完成；调用者须确保析构不与提交/同步调用并发。
 * 不自动重试、跳转、保存 Cookie 或读取代理环境变量；每次请求独立连接。
 * 公共失败仅传递 NetworkError，Result.message 为空。DLL 调用方须使用兼容的 STL/运行时。
 */
class SZX_NETWORK_API HttpClient final
{
public:
    [[nodiscard]] static auto Create(HttpClientOptions options = {}) -> SZX::COMMON::Result<std::unique_ptr<HttpClient>>;
    ~HttpClient();
    HttpClient(const HttpClient &)                     = delete;
    auto operator=(const HttpClient &) -> HttpClient & = delete;
    [[nodiscard]] auto request(std::string method, std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    /** @brief 返回值报告提交失败；成功返回的 future.get() 返回传输结果，不传递后台异常。 */
    [[nodiscard]] auto requestAsync(std::string method, std::string url, HttpRequestOptions options = {})
        -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto get(std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    [[nodiscard]] auto post(std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    [[nodiscard]] auto put(std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    [[nodiscard]] auto patch(std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    [[nodiscard]] auto del(std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    [[nodiscard]] auto head(std::string url, HttpRequestOptions options = {}) const -> HttpClientResult;
    [[nodiscard]] auto options(std::string url, HttpRequestOptions requestOptions = {}) const -> HttpClientResult;

    [[nodiscard]] auto getAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto postAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto putAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto patchAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto delAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto headAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;
    [[nodiscard]] auto optionsAsync(std::string url, HttpRequestOptions options = {}) -> SZX::COMMON::Result<HttpClientFuture>;

private:
    class Impl;
    explicit HttpClient(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

/** @brief 无需创建实例的快捷同步调用；异步调用使用 HttpClient 管理线程及生命周期。 */
namespace http
{
[[nodiscard]] SZX_NETWORK_API auto request(std::string method, std::string url, HttpRequestOptions options = {})
    -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto get(std::string url, HttpRequestOptions options = {}) -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto post(std::string url, HttpRequestOptions options = {}) -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto put(std::string url, HttpRequestOptions options = {}) -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto patch(std::string url, HttpRequestOptions options = {}) -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto del(std::string url, HttpRequestOptions options = {}) -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto head(std::string url, HttpRequestOptions options = {}) -> HttpClientResult;
[[nodiscard]] SZX_NETWORK_API auto options(std::string url, HttpRequestOptions requestOptions = {}) -> HttpClientResult;
} // namespace http
} // namespace v1
} // namespace SZX::NETWORK
