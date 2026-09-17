#pragma once

#include "UserRepository.h"

#include "SzxAdmin/Infra/ErrorCode.h"
#include "TimeHelper.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

/** 用户管理模块共享的请求校验。全部为纯函数，不接触数据库。 */
namespace SZX::ADMIN::USER
{
template <typename T>
[[nodiscard]] inline auto failed(INFRA::ErrorCode code) -> SZX::COMMON::Result<T>
{
    if constexpr (std::is_void_v<T>)
        return SZX::COMMON::Result<void>::Failure(static_cast<int>(code), {});
    else
        return SZX::COMMON::Result<T>::Failure(static_cast<int>(code), {});
}

/** 带连字符的 UUID 布局，十六进制字母不区分大小写。 */
[[nodiscard]] inline auto validUuid(std::string_view value) noexcept -> bool
{
    if (value.size() != 36)
        return false;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const auto character = value[index];
        if (index == 8 || index == 13 || index == 18 || index == 23)
        {
            if (character != '-')
                return false;
        }
        else if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
                   (character >= 'A' && character <= 'F')))
            return false;
    }
    return true;
}

/** UTF-8 码点数；输入已由 JSON 解析器保证是合法 UTF-8。 */
[[nodiscard]] inline auto characterCount(std::string_view value) noexcept -> std::size_t
{
    std::size_t count{};
    for (const unsigned char character : value)
        if ((character & 0xc0) != 0x80)
            ++count;
    return count;
}

[[nodiscard]] inline auto hasControlCharacter(std::string_view value) noexcept -> bool
{
    return std::any_of(value.begin(), value.end(), [](unsigned char character) { return character < 32 || character == 127; });
}

/** 展示名：去空白后非空、无控制字符、不超过列宽 128 个字符。 */
[[nodiscard]] inline auto validDisplayName(std::string_view value) noexcept -> bool
{
    return !value.empty() && !hasControlCharacter(value) && characterCount(value) <= 128 &&
        value.find_first_not_of(" \t") != std::string_view::npos;
}

/** 列表搜索词：无控制字符且不超过 128 个字符。 */
[[nodiscard]] inline auto validSearch(std::string_view value) noexcept -> bool
{
    return !hasControlCharacter(value) && characterCount(value) <= 128;
}

[[nodiscard]] inline auto validAuditResult(std::string_view value) noexcept -> bool
{
    return value == "success" || value == "failure" || value == "denied";
}

/** 审计事件名：小写点分段，匹配 user_logs.event_name 列宽。 */
[[nodiscard]] inline auto validEventName(std::string_view value) noexcept -> bool
{
    if (value.empty() || value.size() > 96 || value.front() == '.' || value.back() == '.')
        return false;
    bool previousDot{};
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const auto character = value[index];
        const bool lower     = character >= 'a' && character <= 'z';
        const bool digit     = character >= '0' && character <= '9';
        if (character == '.')
        {
            if (previousDot)
                return false;
            previousDot = true;
            continue;
        }
        if (!lower && !digit && character != '_')
            return false;
        if (index == 0 && !lower)
            return false;
        previousDot = false;
    }
    return true;
}

/** 请求排序参数到编译期白名单的映射；未知值返回 nullopt。 */
[[nodiscard]] inline auto parseSort(std::string_view value) noexcept -> std::optional<UserSort>
{
    if (value.empty() || value == "-created_at")
        return UserSort::CreatedAtDesc;
    if (value == "created_at")
        return UserSort::CreatedAtAsc;
    if (value == "display_name")
        return UserSort::DisplayNameAsc;
    if (value == "-display_name")
        return UserSort::DisplayNameDesc;
    if (value == "-last_login_at")
        return UserSort::LastLoginAtDesc;
    if (value == "last_login_at")
        return UserSort::LastLoginAtAsc;
    if (value == "role")
        return UserSort::RoleAsc;
    if (value == "status")
        return UserSort::StatusAsc;
    return std::nullopt;
}

/** UTC ISO 8601 转数据库 DATETIME 文本；格式非法返回 nullopt。 */
[[nodiscard]] inline auto databaseTimestamp(std::string_view iso8601) -> std::optional<std::string>
{
    const auto parsed = SZX::COMMON::TimeHelper::Iso8601ToTimestamp(iso8601);
    if (!parsed)
        return std::nullopt;
    return SZX::COMMON::TimeHelper::TimestampToString(*parsed);
}

/** 客户端埋点不得伪造服务端安全审计事件。 */
[[nodiscard]] inline auto reservedEventName(std::string_view value) noexcept -> bool
{
    return value.starts_with("auth.") || value.starts_with("user.");
}

/** 审计行要求可写入的 request_id 和对端地址，匹配 user_logs 列宽。 */
[[nodiscard]] inline auto validContext(std::string_view requestId, std::string_view peerAddress) noexcept -> bool
{
    return !peerAddress.empty() && peerAddress.size() <= 45 && !requestId.empty() && requestId.size() <= 128;
}
} // namespace SZX::ADMIN::USER
