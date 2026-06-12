#pragma once

#include "common/log/log_manager.h"

#include <spdlog/spdlog.h>

#define LOG_TRACE(...) ::LogManager::instance().logger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::LogManager::instance().logger()->debug(__VA_ARGS__)
#define LOG_INFO(...) ::LogManager::instance().logger()->info(__VA_ARGS__)
#define LOG_WARN(...) ::LogManager::instance().logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::LogManager::instance().logger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::LogManager::instance().logger()->critical(__VA_ARGS__)
