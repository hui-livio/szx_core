#pragma once

#include "SzxAdmin/Infra/HttpTransport.h"

#include <span>
#include <string>
#include <string_view>

/** 浏览器可见路由共享的 CORS 和查询参数校验。策略由 admin 层注入，网络库不内置。 */
namespace SZX::ADMIN
{
/** Origin 白名单；"*" 接受任意来源，无 Origin 的原生客户端始终允许。 */
[[nodiscard]] auto originAllowed(std::string_view allowedOrigin, const INFRA::HttpRequest &request) -> bool;

/** 回传具体 Origin；携带凭据的请求不返回通配符响应头。 */
[[nodiscard]] auto withCors(INFRA::HttpResponse response, const INFRA::HttpRequest &request) -> INFRA::HttpResponse;

/** 构造仅允许 methods 的 OPTIONS 预检处理器；methods 形如 "GET, POST"。 */
[[nodiscard]] auto preflightHandler(std::string allowedOrigin, std::string methods) -> INFRA::HttpHandler;

/** 拒绝白名单外的查询参数，避免拼写错误被静默忽略。 */
[[nodiscard]] auto allowedQuery(const INFRA::HttpRequest &request, std::span<const std::string_view> names) -> bool;

/** 读取查询参数，缺失时返回空字符串。 */
[[nodiscard]] auto queryValue(const INFRA::HttpRequest &request, std::string_view name) -> std::string;
} // namespace SZX::ADMIN
