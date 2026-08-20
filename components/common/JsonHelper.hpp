/**
 * @file JsonHelper.hpp
 * @brief 常用 JSON 处理工具。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-02
 */

#pragma once

#include "Serializer.hpp"
#include "StringHelper.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace SZX::COMMON
{

/**
 * @class JsonHelper
 * @brief 提供 JSON 解析、深度合并和常用类型转换。
 */
class JsonHelper final
{
public:
    /**
     * @brief 安全解析任意 JSON 值。
     * @return 解析成功时返回 JSON 值，输入无效时返回 std::nullopt。
     */
    static auto Parse(std::string_view text) -> std::optional<Json>
    {
        if (text.empty())
            return std::nullopt;
        try
        {
            return Json::parse(text.begin(), text.end());
        } catch (...)
        {
            return std::nullopt;
        }
    }

    /**
     * @brief 解析 JSON object。
     * @param text JSON 字符串。
     * @return JSON object；解析失败或不是 object 时返回空 object。
     */
    static auto ParseObject(const std::optional<std::string> &text) -> Json
    {
        if (!text || text->empty())
            return Json::object();
        return ParseObject(*text);
    }

    /**
     * @brief 解析 JSON object。
     * @param text JSON 字符串。
     * @return JSON object；解析失败或不是 object 时返回空 object。
     */
    static auto ParseObject(const char *text) -> Json
    {
        return ParseObject(std::string(text == nullptr ? "" : text));
    }

    /**
     * @brief 解析 JSON object。
     * @param text JSON 字符串。
     * @return JSON object；解析失败或不是 object 时返回空 object。
     */
    static auto ParseObject(const std::string &text) -> Json
    {
        const auto value = Parse(text);
        return value && value->is_object() ? *value : Json::object();
    }

    /**
     * @brief 解析必需的 JSON object。
     * @param text JSON 字符串。
     * @return JSON object；解析失败或不是 object 时返回 std::nullopt。
     */
    static auto ParseRequiredObject(const std::string &text) -> std::optional<Json>
    {
        auto value = Parse(text);
        if (value && value->is_object())
            return value;
        return std::nullopt;
    }

    /**
     * @brief 深度合并 JSON object。
     * @param base 基础 object。
     * @param patch 补丁 object。
     * @return 合并后的 object。
     *
     * 两侧同名字段均为 object 时递归合并，否则 patch 覆盖 base。
     */
    static auto DeepMerge(Json base, const Json &patch) -> Json
    {
        if (!base.is_object())
            base = Json::object();
        if (!patch.is_object())
            return base;

        for (auto it = patch.begin(); it != patch.end(); ++it)
        {
            if (it.value().is_object() && base.contains(it.key()) && base[it.key()].is_object())
                base[it.key()] = DeepMerge(base[it.key()], it.value());
            else
                base[it.key()] = it.value();
        }
        return base;
    }

    /**
     * @brief 将 JSON 值转换为可选字符串。
     * @param value JSON 值。
     * @return null 或空 object 返回 std::nullopt，其余返回 dump() 字符串。
     */
    static auto ToOptionalString(const Json &value) -> std::optional<std::string>
    {
        if (value.is_null() || (value.is_object() && value.empty()))
            return std::nullopt;
        return value.dump();
    }

    /**
     * @brief 将 JSON 值转换为字符串。
     * @param value JSON 值。
     * @return string 类型返回原值，null 返回空字符串，其余返回 dump() 字符串。
     */
    static auto ToString(const Json &value) -> std::string
    {
        if (value.is_string())
            return value.get<std::string>();
        if (value.is_null())
            return {};
        return value.dump();
    }

    /**
     * @brief 将 JSON 值转换为 int。
     * @param value JSON 值。
     * @param defaultValue 转换失败时的默认值。
     * @return 转换结果。
     */
    static auto ToInt(const Json &value, int defaultValue) -> int
    {
        try
        {
            if (value.is_number_integer())
                return value.get<int>();
            if (value.is_number_float())
                return static_cast<int>(value.get<double>());
            if (value.is_string())
            {
                const auto text = StringHelper::Strip(value.get<std::string>());
                if (const auto parsed = StringHelper::ParseInteger<int>(text))
                    return *parsed;
            }
        } catch (...)
        {
        }
        return defaultValue;
    }

    /**
     * @brief 严格读取 JSON boolean。
     * @return value 为 boolean 时返回其值，否则返回 std::nullopt。
     */
    static auto ToOptionalBoolStrict(const Json &value) -> std::optional<bool>
    {
        if (!value.is_boolean())
            return std::nullopt;
        return value.get<bool>();
    }

    /**
     * @brief 将常见配置值宽松转换为 boolean。
     *
     * 支持 boolean、有限数字以及英文配置字符串。面向具体产品的本地化
     * 别名应由业务层在调用前归一化，避免通用组件携带领域语言策略。
     */
    static auto ToOptionalBoolCoerced(const Json &value) -> std::optional<bool>
    {
        if (const auto strict = ToOptionalBoolStrict(value))
            return strict;
        if (value.is_number())
        {
            const auto number = value.get<double>();
            return std::isfinite(number) ? std::optional<bool>{number != 0.0} : std::nullopt;
        }
        if (!value.is_string())
            return std::nullopt;

        const auto text = StringHelper::ToLower(StringHelper::Strip(value.get<std::string>()));
        if (text == "true" || text == "1" || text == "yes" || text == "on" || text == "enabled" || text == "enable")
            return true;
        if (text == "false" || text == "0" || text == "no" || text == "off" || text == "disabled" || text == "disable")
            return false;
        return std::nullopt;
    }

    /**
     * @brief 读取有限 JSON 数字。
     * @return value 为有限数字时返回 double，否则返回 std::nullopt。
     */
    static auto ToFiniteDouble(const Json &value) -> std::optional<double>
    {
        if (!value.is_number())
            return std::nullopt;
        const auto number = value.get<double>();
        return std::isfinite(number) ? std::optional<double>{number} : std::nullopt;
    }
};

} // namespace SZX::COMMON
