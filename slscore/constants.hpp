#ifndef _CONSTANTS_INCLUDE_
#define _CONSTANTS_INCLUDE_

#include "spdlog/spdlog.h"

#define DEFAULT_PIDFILE "/tmp/sls/sls_server.pid"

#ifdef NDEBUG
    static const spdlog::level::level_enum DEFAULT_LOG_LEVEL = spdlog::level::info;
#else
    static const spdlog::level::level_enum DEFAULT_LOG_LEVEL = spdlog::level::debug;
#endif

#endif