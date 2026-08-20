/**
 * @file Result.hpp
 * @brief   通用结果包装 — Result<T> / Result<void>，承载成功/失败状态、错误码及消息
 * @details 用于跨层返回操作结果，支持泛型成功值和统一错误处理
 * @author  livliu (yuanfeng1897@outlook.com)
 * @date    2026-06-10
 * @version 1.0
 */

#pragma once

#include <string>
#include <type_traits>
#include <utility>

namespace SZX::COMMON
{

/**
 * @class   Result
 * @brief   通用结果包装模板 — 承载成功/失败状态、错误码及消息
 * @details 用于跨层返回操作结果，支持泛型成功值和统一错误处理。
 *          典型使用场景：
 *          - 函数返回值包装（替代异常或错误码）
 *          - API 层间通信的统一响应格式
 *
 * @tparam   T 成功时的值类型（void 特化版本用于无返回值的场景）
 *
 * @example
 * @code
 * Result<int> result = FunctionThatMayFail();
 * if (result.ok) {
 *     UseValue(result.value);
 * } else {
 *     HandleError(result.error, result.message);
 * }
 * @endcode
 */
template <typename T>
class Result
{
public:
    static constexpr bool canCreateFailureWithoutValue = std::is_default_constructible_v<T>;

    bool ok;             ///< 操作是否成功
    int error;           ///< 错误码（ok=false 时有效）
    std::string message; ///< 错误消息（ok=false 时有效）
    T value;             ///< 成功时的返回值（ok=true 时有效）

    /**
     * @brief   构建成功结果
     *
     * @param   val 成功时的返回值（支持移动语义）
     *
     * @return  Result 实例（ok=true, error=Success, message=""）
     */
    static Result<T> Success(T val)
    {
        return Result<T>{.ok = true, .error = 0, .message = "", .value = std::move(val)};
    }

    /**
     * @brief   构建失败结果
     *
     * @param   err 错误码
     * @param   msg 错误描述消息
     *
     * @return  Result 实例（ok=false, value 为默认构造）
     */
    static Result<T> Failure(int err, const std::string &msg)
    {
        static_assert(std::is_default_constructible_v<T>, "Result<T>::Failure requires T to be default constructible.");
        return Result<T>{.ok = false, .error = err, .message = msg, .value = T{}};
    }
};

/**
 * @class   Result<void>
 * @brief   Result 的 void 特化版本 — 用于无返回值的操作结果
 * @details 与 Result<T> 接口一致，但不包含 value 字段
 */
template <>
class Result<void>
{
public:
    bool ok;             ///< 操作是否成功
    int error;           ///< 错误码（ok=false 时有效）
    std::string message; ///< 错误消息（ok=false 时有效）

    /**
     * @brief   构建成功的 void 结果
     *
     * @return  Result<void> 实例（ok=true）
     */
    static Result<void> Success()
    {
        return Result<void>{.ok = true, .error = 0, .message = ""};
    }

    /**
     * @brief   构建失败的 void 结果
     *
     * @param   err 错误码
     * @param   msg 错误描述消息
     *
     * @return  Result<void> 实例（ok=false）
     */
    static Result<void> Failure(int err, const std::string &msg)
    {
        return Result<void>{.ok = false, .error = err, .message = msg};
    }
};
} // namespace SZX::COMMON
