/**
 * @file StringHelper.hpp
 * @brief 常用字符串处理工具。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-02
 */

#pragma once

#include <cctype>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace SZX::COMMON
{

/**
 * @class StringHelper
 * @brief 提供 strip、split、join 等无状态字符串工具函数。
 */
class StringHelper final
{
public:
    /**
     * @brief 将字符串转换为小写。
     * @note 按当前 C locale 对单字节字符转换，适合标识符、扩展名等 ASCII 文本。
     */
    static auto ToLower(std::string_view text) -> std::string
    {
        auto result = std::string(text);
        for (auto &ch : result)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return result;
    }

    /**
     * @brief 将字符串转换为大写。
     * @note 按当前 C locale 对单字节字符转换，适合枚举名、SQL 关键字等 ASCII 文本。
     */
    static auto ToUpper(std::string_view text) -> std::string
    {
        auto result = std::string(text);
        for (auto &ch : result)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return result;
    }

    /**
     * @brief 严格解析整数，要求整个输入都属于整数。
     * @param text 待解析文本；不会自动去除空白。
     * @param base 进制，取值范围与 std::from_chars 相同。
     * @return 解析成功时返回整数，否则返回 std::nullopt。
     */
    template <std::integral Integer>
        requires(!std::same_as<std::remove_cv_t<Integer>, bool>)
    static auto ParseInteger(std::string_view text, int base = 10) -> std::optional<Integer>
    {
        if (text.empty())
            return std::nullopt;

        Integer value{};
        const auto *begin = text.data();
        const auto *end   = begin + text.size();
        const auto parsed = std::from_chars(begin, end, value, base);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            return std::nullopt;
        return value;
    }

    /**
     * @brief 去除字符串左侧空白字符。
     * @param text 输入字符串。
     * @return 去除左侧空白后的字符串。
     */
    static auto LStrip(std::string_view text) -> std::string
    {
        auto first = std::size_t{0};
        while (first < text.size() && IsAsciiWhitespace(text[first]))
            ++first;
        return std::string(text.substr(first));
    }

    /**
     * @brief 去除字符串右侧空白字符。
     * @param text 输入字符串。
     * @return 去除右侧空白后的字符串。
     */
    static auto RStrip(std::string_view text) -> std::string
    {
        auto last = text.size();
        while (last > 0 && IsAsciiWhitespace(text[last - 1]))
            --last;
        return std::string(text.substr(0, last));
    }

    /**
     * @brief 去除字符串两侧空白字符。
     * @param text 输入字符串。
     * @return 去除两侧空白后的字符串。
     */
    static auto Strip(std::string_view text) -> std::string
    {
        auto first = std::size_t{0};
        auto last  = text.size();
        while (first < last && IsAsciiWhitespace(text[first]))
            ++first;
        while (last > first && IsAsciiWhitespace(text[last - 1]))
            --last;
        return std::string(text.substr(first, last - first));
    }

    /**
     * @brief 按单字符分隔字符串。
     * @param text 输入字符串。
     * @param delimiter 分隔符。
     * @param keepEmpty 是否保留空字段。
     * @return 分隔后的字符串列表。
     */
    static auto Split(std::string_view text, char delimiter, bool keepEmpty = true) -> std::vector<std::string>
    {
        std::vector<std::string> result;
        auto start = std::size_t{0};
        while (start <= text.size())
        {
            const auto pos = text.find(delimiter, start);
            const auto end = pos == std::string_view::npos ? text.size() : pos;
            if (keepEmpty || end > start)
                result.emplace_back(text.substr(start, end - start));
            if (pos == std::string_view::npos)
                break;
            start = pos + 1;
        }
        return result;
    }

    /**
     * @brief 按字符串分隔符分隔字符串。
     * @param text 输入字符串。
     * @param delimiter 分隔符；为空时返回原字符串。
     * @param keepEmpty 是否保留空字段。
     * @return 分隔后的字符串列表。
     */
    static auto Split(std::string_view text, std::string_view delimiter, bool keepEmpty = true) -> std::vector<std::string>
    {
        if (delimiter.empty())
            return {std::string(text)};

        std::vector<std::string> result;
        auto start = std::size_t{0};
        while (start <= text.size())
        {
            const auto pos = text.find(delimiter, start);
            const auto end = pos == std::string_view::npos ? text.size() : pos;
            if (keepEmpty || end > start)
                result.emplace_back(text.substr(start, end - start));
            if (pos == std::string_view::npos)
                break;
            start = pos + delimiter.size();
        }
        return result;
    }

    /**
     * @brief 使用分隔符拼接字符串列表。
     * @param parts 字符串列表。
     * @param delimiter 分隔符。
     * @return 拼接后的字符串。
     */
    static auto Join(const std::vector<std::string> &parts, std::string_view delimiter) -> std::string
    {
        std::ostringstream output;
        for (auto index = std::size_t{0}; index < parts.size(); ++index)
        {
            if (index > 0)
                output << delimiter;
            output << parts[index];
        }
        return output.str();
    }

    /**
     * @brief 使用分隔符拼接字符串视图列表。
     * @param parts 字符串视图列表。
     * @param delimiter 分隔符。
     * @return 拼接后的字符串。
     */
    static auto Join(const std::vector<std::string_view> &parts, std::string_view delimiter) -> std::string
    {
        std::ostringstream output;
        for (auto index = std::size_t{0}; index < parts.size(); ++index)
        {
            if (index > 0)
                output << delimiter;
            output << parts[index];
        }
        return output.str();
    }

    /**
     * @brief 判断单字节字符是否属于 ASCII 空白集合。
     *
     * 适用于文件名、协议字段等按 ASCII 规则处理的文本，且不依赖当前 C locale。
     */
    static auto IsAsciiWhitespace(char ch) -> bool
    {
        switch (ch)
        {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\f':
        case '\v':
            return true;
        default:
            return false;
        }
    }
};

} // namespace SZX::COMMON
