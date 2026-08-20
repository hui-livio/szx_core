/**
 * @file MessageBus.hpp
 * @brief   消息总线适配层 — 将 msgbus 核心类型引入 SZX::COMMON 命名空间
 * @details 提供零开销的类型别名，编译期解析为原始 msgbus 类型
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include "ext/msgbus/message_bus.h"

namespace SZX::COMMON
{

// ========================================================================
// 核心类 — 直接重命名（零开销别名）
// ========================================================================

// 消息总线核心类 — 支持主题发布/订阅、通配符匹配
using MessageBus = msgbus::MessageBus;

// 主题注册表 — 管理所有 Topic 和 Subscription 的映射关系
using TopicRegistry = msgbus::TopicRegistry;

// ========================================================================
// 类型别名
// ========================================================================

// 主题唯一标识符类型
using TopicId = msgbus::TopicId;

// 订阅句柄类型（用于取消订阅）
using SubscriptionId = msgbus::SubscriptionId;

// 消息投递策略枚举
using FullPolicy = msgbus::FullPolicy;

// 消息智能指针类型
using MessagePtr = msgbus::MessagePtr;

// 消息抽象接口
using IMessage = msgbus::IMessage;

// ========================================================================
// 常量定义
// ========================================================================

// 无效的主题 ID（用于错误检查）
inline constexpr TopicId kInvalidTopicId = msgbus::kInvalidTopicId;

// 默认的消息队列容量
inline constexpr size_t kDefaultQueueCapacity = msgbus::kDefaultQueueCapacity;

} // namespace SZX::COMMON
