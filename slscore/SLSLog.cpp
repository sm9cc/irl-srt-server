
/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Edward.Wu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <mutex>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <strings.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "SLSLog.hpp"
#include "SLSLock.hpp"


std::mutex LOGGER_MUTEX;

#ifdef NDEBUG
    static const spdlog::level::level_enum DEFAULT_LOG_LEVEL = spdlog::level::info;
#else
    static const spdlog::level::level_enum DEFAULT_LOG_LEVEL = spdlog::level::debug;
#endif

int initialize_logger()
{
    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    sinks.push_back(console_sink);

    auto combined_logger = std::make_shared<spdlog::logger>(APP_NAME, begin(sinks), end(sinks));
    combined_logger->set_level(DEFAULT_LOG_LEVEL);

    spdlog::set_default_logger(combined_logger);

    return 0;
}

int sls_set_log_level(char *log_level)
{
    log_level = sls_strupper(log_level); //to upper
    int n = sizeof(LOG_LEVEL_NAME) / sizeof(char *);
    for (int i = 0; i < n; i++)
    {
        if (strcmp(log_level, LOG_LEVEL_NAME[i]) == 0)
        {
            spdlog::get(APP_NAME)->set_level((spdlog::level::level_enum)i);
            spdlog::info("set log level='{}'.", LOG_LEVEL_NAME[i]);
            return 0;
        }
    }
    spdlog::warn("!!!wrong log level '{}', set default '{}'.", log_level, LOG_LEVEL_NAME[DEFAULT_LOG_LEVEL]);
    return 1;
}

int sls_set_log_file(char *log_file)
{
    if (log_file && strlen(log_file) > 0)
    {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file);
        LOGGER_MUTEX.lock();
        spdlog::get(APP_NAME)->sinks().push_back(file_sink);
        LOGGER_MUTEX.unlock();
        return SLS_OK;
    }
    else
    {
        return SLS_ERROR;
    }
}
