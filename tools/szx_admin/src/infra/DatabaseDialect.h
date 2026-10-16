#pragma once

#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace SZX::ADMIN::INFRA::DETAIL
{
/** Private server capability selection; not a user-controlled configuration switch. */
enum class DatabaseDialect
{
    MySql8,
    MariaDb
};

/** Supported baseline: MySQL 8.0.16+ (CHECK enforcement), MariaDB 10.11+.
 * Reject unknown/old engines rather than silently weaken schema constraints. */
[[nodiscard]] inline auto databaseDialect(std::string_view version) noexcept -> std::optional<DatabaseDialect>
{
    const bool maria = version.find("MariaDB") != std::string_view::npos;
    if (maria && version.starts_with("5.5.5-"))
        version.remove_prefix(6);
    std::array<unsigned, 3> parts{};
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        const auto [end, error] = std::from_chars(version.data(), version.data() + version.size(), parts[i]);
        if (error != std::errc{})
            return std::nullopt;
        version.remove_prefix(static_cast<std::size_t>(end - version.data()));
        if (i < 2)
        {
            if (version.empty() || version.front() != '.')
                return std::nullopt;
            version.remove_prefix(1);
        }
    }
    if (!version.empty() && version.front() != '-')
        return std::nullopt;
    if (maria)
        return (parts[0] > 10 || (parts[0] == 10 && parts[1] >= 11)) ? std::optional{DatabaseDialect::MariaDb} : std::nullopt;
    if (parts[0] == 8 && (parts[1] > 0 || parts[2] >= 16))
        return DatabaseDialect::MySql8;
    return std::nullopt;
}

/** Adapt only trusted initialization DDL at execution time, never bound application queries.
 * Published migration bytes/checksums remain unchanged. Quoted values/identifiers and comments
 * are copied verbatim; no regex replacement in user values. May throw internally on allocation. */
[[nodiscard]] inline auto initializationSql(std::string_view sql, DatabaseDialect dialect) -> std::string
{
    if (dialect == DatabaseDialect::MySql8)
        return std::string(sql);
    std::string output;
    output.reserve(sql.size());
    bool drop = false;
    for (std::size_t i = 0; i < sql.size();)
    {
        const char ch = sql[i];
        if (ch == '\'' || ch == '"' || ch == '`')
        {
            drop             = false;
            const auto start = i++;
            while (i < sql.size())
            {
                if (sql[i] == '\\' && i + 1 < sql.size())
                {
                    i += 2;
                    continue;
                }
                if (sql[i++] == ch)
                {
                    if (i < sql.size() && sql[i] == ch)
                    {
                        ++i;
                        continue;
                    }
                    break;
                }
            }
            output.append(sql.substr(start, i - start));
        }
        else if ((ch == '-' && i + 1 < sql.size() && sql[i + 1] == '-') || ch == '#' ||
                 (ch == '/' && i + 1 < sql.size() && sql[i + 1] == '*'))
        {
            const bool block  = ch == '/';
            const auto end    = block ? sql.find("*/", i + 2) : sql.find('\n', i + 1);
            const auto length = end == std::string_view::npos ? sql.size() - i : end - i + (block ? 2 : 1);
            output.append(sql.substr(i, length));
            i += length;
        }
        else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_')
        {
            const auto start = i++;
            while (i < sql.size() &&
                   ((sql[i] >= 'a' && sql[i] <= 'z') || (sql[i] >= 'A' && sql[i] <= 'Z') || (sql[i] >= '0' && sql[i] <= '9') ||
                    sql[i] == '_'))
                ++i;
            const auto word = sql.substr(start, i - start);
            if (word == "utf8mb4_0900_ai_ci")
                output += "utf8mb4_unicode_520_ci";
            else if (word == "utf8mb4_0900_bin")
                output += "utf8mb4_nopad_bin";
            else if (drop && word == "CHECK")
                output += "CONSTRAINT";
            else
                output.append(word);
            drop = word == "DROP";
        }
        else
        {
            output += ch;
            ++i;
            if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
                drop = false;
        }
    }
    return output;
}
} // namespace SZX::ADMIN::INFRA::DETAIL
