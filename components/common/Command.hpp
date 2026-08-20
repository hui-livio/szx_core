/**
 * @file Command.hpp
 * @brief 命令对象封装 — 提供可异步执行的轻量命令类型。
 *
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include "cli11/CLI11.hpp"


namespace SZX::COMMON
{
using Command = CLI::App;
}

#define CMD_PARSE CLI11_PARSE
