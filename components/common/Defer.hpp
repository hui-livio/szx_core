/**
 * @file Defer.hpp
 * @brief Go-style defer — RAII scope guard，离开作用域自动执行清理逻辑
 * @details 与 Go 语言 defer 语句类似，用于在离开作用域时执行清理操作。
 * @note 该类是 RAII 类，离开作用域时会自动调用清理函数。
 * @note 可以使用便捷宏 defer { cleanup(); } 来简化使用。
 * @example
 * @code
 *  void ReadFile(const char* path) {
 *      FILE* f = fopen(path, "r");
 *      defer { fclose(f); };
 *      // ... use f ...
 *  }  // fclose(f) 在此自动调用。
 * @endcode
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>

namespace SZX::COMMON
{

template <typename Func>
class Defer
{
public:
    explicit Defer(Func &&f) noexcept(std::is_nothrow_move_constructible_v<Func>) :
        func_(std::forward<Func>(f)),
        active_(true)
    {
    }

    ~Defer() noexcept
    {
        if (active_)
        {
            try
            {
                func_();
            } catch (const std::exception &e)
            {
                std::cerr << "Defer cleanup failed: " << e.what() << '\n';
            } catch (...)
            {
                std::cerr << "Defer cleanup failed: unknown exception\n";
            }
        }
    }

    Defer(const Defer &)            = delete;
    Defer &operator=(const Defer &) = delete;

    Defer(Defer &&other) noexcept(std::is_nothrow_move_constructible_v<Func>) :
        func_(std::move(other.func_)),
        active_(std::exchange(other.active_, false))
    {
    }

    Defer &operator=(Defer &&other) = delete;

    void Cancel() noexcept
    {
        active_ = false;
    }

private:
    Func func_;
    bool active_;
};

template <typename Func>
Defer(Func) -> Defer<Func>;


// ========================================================================
// defer { ... } 语法支持
// ========================================================================
struct DeferTag
{
};
constexpr DeferTag defer_tag{};

template <typename Func>
[[nodiscard]] Defer<std::decay_t<Func>> operator*(DeferTag, Func &&f)
{
    return Defer<std::decay_t<Func>>(std::forward<Func>(f));
}

} // namespace SZX::COMMON


/**
 * @brief 便捷宏: defer { cleanup(); };
 * @code
 *  void ReadFile(const char* path) {
 *      FILE* f = fopen(path, "r");
 *      defer { fclose(f); };
 *      // ... use f ...
 *  }  // fclose(f) 在此自动调用
 * @endcode
 */
#define DEFER_IMPL_CONCAT_(a, b) a##b
#define DEFER_IMPL_CONCAT(a, b) DEFER_IMPL_CONCAT_(a, b)
#define defer auto DEFER_IMPL_CONCAT(_defer_, __COUNTER__) = ::SZX::COMMON::defer_tag *[&]
