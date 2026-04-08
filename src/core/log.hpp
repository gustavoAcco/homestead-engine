#pragma once

// Private logging facade for homestead::core implementation files.
// MUST NOT be included from any public header under include/homestead/.
// Constitution Principle II: homestead::core must compile with stdlib only;
// spdlog is an optional private dependency compiled in only when
// HOMESTEAD_ENABLE_LOGGING is defined.

#ifdef HOMESTEAD_ENABLE_LOGGING
#include <spdlog/spdlog.h>
#define HOMESTEAD_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define HOMESTEAD_LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define HOMESTEAD_LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#else
#define HOMESTEAD_LOG_DEBUG(...) \
    do {                         \
    } while (false)
#define HOMESTEAD_LOG_INFO(...) \
    do {                        \
    } while (false)
#define HOMESTEAD_LOG_WARN(...) \
    do {                        \
    } while (false)
#endif
