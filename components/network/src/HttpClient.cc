#include "SzxNetwork/HttpClient.h"
#include "Defer.hpp"
#include "JsonHelper.hpp"
#include "ThreadPool.hpp"

#include <atomic>
#include <utility>

#if SZX_NETWORK_HAS_HTTP_CLIENT
    #include "detail/HttpClientTransport.h"
#endif

namespace SZX::NETWORK::v1
{
namespace
{
template <typename T>
auto failed(NetworkError error) -> SZX::COMMON::Result<T>
{
    return SZX::COMMON::Result<T>::Failure(static_cast<int>(error), {});
}
} // namespace

auto HttpClientResponse::ok() const noexcept -> bool
{
    return statusCode >= 200 && statusCode < 400;
}
auto HttpClientResponse::text() const noexcept -> const std::string &
{
    return body;
}
auto HttpClientResponse::json() const -> SZX::COMMON::Result<SZX::COMMON::Json>
{
    try
    {
        auto parsed = SZX::COMMON::JsonHelper::Parse(body);
        return parsed ? SZX::COMMON::Result<SZX::COMMON::Json>::Success(std::move(*parsed)) :
                        failed<SZX::COMMON::Json>(NetworkError::ProtocolError);
    } catch (const std::bad_alloc &)
    {
        return failed<SZX::COMMON::Json>(NetworkError::ResourceExhausted);
    } catch (...)
    {
        return failed<SZX::COMMON::Json>(NetworkError::Internal);
    }
}

class HttpClient::Impl final
{
public:
    explicit Impl(HttpClientOptions value) :
        options(value),
        pool(value.workerCount, "http-client")
    {
    }
    HttpClientOptions options;
    std::atomic_size_t pending{};
    SZX::COMMON::ThreadPool pool; // Destroy first: accepted tasks may still access pending.
};

HttpClient::HttpClient(std::unique_ptr<Impl> impl) :
    impl_(std::move(impl))
{
}
HttpClient::~HttpClient() = default;

auto HttpClient::Create(HttpClientOptions options) -> SZX::COMMON::Result<std::unique_ptr<HttpClient>>
{
    try
    {
        if (options.workerCount == 0 || options.workerCount > 64 || options.maxPendingRequests == 0)
            return failed<std::unique_ptr<HttpClient>>(NetworkError::InvalidArgument);
#if !SZX_NETWORK_HAS_HTTP_CLIENT
        return failed<std::unique_ptr<HttpClient>>(NetworkError::HttpClientUnavailable);
#else
        return SZX::COMMON::Result<std::unique_ptr<HttpClient>>::Success(
            std::unique_ptr<HttpClient>(new HttpClient(std::make_unique<Impl>(options))));
#endif
    } catch (const std::bad_alloc &)
    {
        return failed<std::unique_ptr<HttpClient>>(NetworkError::ResourceExhausted);
    } catch (...)
    {
        return failed<std::unique_ptr<HttpClient>>(NetworkError::Internal);
    }
}

auto http::request(std::string method, std::string url, HttpRequestOptions options) -> HttpClientResult
{
#if SZX_NETWORK_HAS_HTTP_CLIENT
    return detail::sendHttpRequest(std::move(method), std::move(url), std::move(options));
#else
    (void)method;
    (void)url;
    (void)options;
    return failed<HttpClientResponse>(NetworkError::HttpClientUnavailable);
#endif
}

auto HttpClient::request(std::string method, std::string url, HttpRequestOptions options) const -> HttpClientResult
{
    return http::request(std::move(method), std::move(url), std::move(options));
}

auto HttpClient::requestAsync(std::string method, std::string url, HttpRequestOptions options)
    -> SZX::COMMON::Result<HttpClientFuture>
{
    try
    {
        auto count = impl_->pending.load();
        do
        {
            if (count >= impl_->options.maxPendingRequests)
                return failed<HttpClientFuture>(NetworkError::SendQueueFull);
        } while (!impl_->pending.compare_exchange_weak(count, count + 1));
        bool submitted = false;
        defer
        {
            if (!submitted)
                --impl_->pending;
        };
        auto future = impl_->pool.submit(
            [state = impl_.get(), method = std::move(method), url = std::move(url), options = std::move(options)]() mutable
            {
                defer
                {
                    --state->pending;
                };
                return http::request(std::move(method), std::move(url), std::move(options));
            });
        submitted = true;
        return SZX::COMMON::Result<HttpClientFuture>::Success(std::move(future));
    } catch (const std::bad_alloc &)
    {
        return failed<HttpClientFuture>(NetworkError::ResourceExhausted);
    } catch (...)
    {
        return failed<HttpClientFuture>(NetworkError::Internal);
    }
}

// All convenience calls share exactly the same validation and transport semantics.
#define SZX_HTTP_METHOD(name, verb)                                                                                              \
    auto http::name(std::string url, HttpRequestOptions options) -> HttpClientResult                                             \
    {                                                                                                                            \
        return http::request(verb, std::move(url), std::move(options));                                                          \
    }                                                                                                                            \
    auto HttpClient::name(std::string url, HttpRequestOptions options) const -> HttpClientResult                                 \
    {                                                                                                                            \
        return request(verb, std::move(url), std::move(options));                                                                \
    }                                                                                                                            \
    auto HttpClient::name##Async(std::string url, HttpRequestOptions options)->SZX::COMMON::Result<HttpClientFuture>             \
    {                                                                                                                            \
        return requestAsync(verb, std::move(url), std::move(options));                                                           \
    }
SZX_HTTP_METHOD(get, "GET")
SZX_HTTP_METHOD(post, "POST")
SZX_HTTP_METHOD(put, "PUT")
SZX_HTTP_METHOD(patch, "PATCH")
SZX_HTTP_METHOD(del, "DELETE")
SZX_HTTP_METHOD(head, "HEAD")
SZX_HTTP_METHOD(options, "OPTIONS")
#undef SZX_HTTP_METHOD
} // namespace SZX::NETWORK::v1
