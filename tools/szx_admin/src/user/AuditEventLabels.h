#pragma once

#include <array>
#include <string_view>
#include <utility>

namespace SZX::ADMIN::USER
{
/** 中文名称用于审计事件关键词检索；未知事件仍可按原始代码搜索。 */
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 17> kAuditEventLabels{{
    {"auth.login", "用户登录"},
    {"auth.login.success", "登录成功"},
    {"auth.login.failure", "登录失败"},
    {"auth.login.rate_limited", "登录请求被限流"},
    {"auth.account.locked", "账号被锁定"},
    {"auth.logout", "退出登录"},
    {"auth.refresh.success", "会话刷新成功"},
    {"auth.refresh.failure", "会话刷新失败"},
    {"auth.refresh.replayed", "检测到刷新令牌重放"},
    {"auth.password.changed", "修改密码成功"},
    {"auth.password.change_failed", "修改密码失败"},
    {"user.created", "创建用户"},
    {"user.updated", "修改用户信息"},
    {"user.status.changed", "变更用户状态"},
    {"user.unlocked", "解锁用户"},
    {"user.role.changed", "变更用户角色"},
    {"user.password.reset", "重置用户密码"},
}};
} // namespace SZX::ADMIN::USER
