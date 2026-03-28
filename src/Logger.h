#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace Logger {

    inline void init() {
        auto console = spdlog::stdout_color_mt("openmixer");
        spdlog::set_default_logger(console);
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%-5l%$] %v");
    }

}  // namespace Logger

// Convenience macros — use these everywhere instead of calling spdlog directly.
#define LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
