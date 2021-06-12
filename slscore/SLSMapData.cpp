
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "spdlog/spdlog.h"

#include "SLSMapData.hpp"
#include "SLSLog.hpp"

/**
 * CSLSMapData class implementation
 */

CSLSMapData::CSLSMapData()
{
}
CSLSMapData::~CSLSMapData()
{
    clear();
}

int CSLSMapData::add(char *key)
{
    int ret = SLS_OK;
    std::string strKey = std::string(key);

    CSLSLock lock(&m_rwclock, true);

    std::map<std::string, CSLSRecycleArray *>::iterator item;
    item = m_map_array.find(strKey);
    if (item != m_map_array.end())
    {
        CSLSRecycleArray *array_data = item->second;
        if (array_data)
        {
            spdlog::info("[{}] CSLSMapData::add, failed, key={}, array_data={}, exist.",
                         fmt::ptr(this), key, fmt::ptr(array_data));
            return ret;
        }
        //m_map_array.erase(item);
    }

    CSLSRecycleArray *data_array = new CSLSRecycleArray;
    //m_map_array.insert(make_pair(strKey, data_array));
    m_map_array[strKey] = data_array;
    spdlog::info("[{}] CSLSMapData::add ok, key='{}'.",
                 fmt::ptr(this), key);
    return ret;
}

int CSLSMapData::remove(char *key)
{
    int ret = SLS_ERROR;
    std::string strKey = std::string(key);

    CSLSLock lock(&m_rwclock, true);

    std::map<std::string, CSLSRecycleArray *>::iterator item;
    item = m_map_array.find(strKey);
    if (item != m_map_array.end())
    {
        CSLSRecycleArray *array_data = item->second;
        spdlog::info("[{}] CSLSMapData::remove, key='{}' delete array_data={}.",
                     fmt::ptr(this), key, fmt::ptr(array_data));
        if (array_data)
        {
            delete array_data;
        }
        m_map_array.erase(item);
        return SLS_OK;
    }
    return ret;
}

bool CSLSMapData::is_exist(char *key)
{

    CSLSLock lock(&m_rwclock, true);
    std::string strKey = std::string(key);

    std::map<std::string, CSLSRecycleArray *>::iterator item;
    item = m_map_array.find(key);
    if (item != m_map_array.end())
    {
        CSLSRecycleArray *array_data = item->second;
        if (array_data)
        {
            spdlog::trace("[{}] CSLSMapData::is_exist, key={}, exist.",
                          fmt::ptr(this), key);
            return true;
        }
        else
        {
            spdlog::trace("[{}] CSLSMapData::is_exist, is_exist, key={}, data_array is null.",
                          fmt::ptr(this), key);
        }
    }
    else
    {
        spdlog::trace("[{}] CSLSMapData::add, is_exist, key={}, not exist.",
                      fmt::ptr(this), key);
    }
    return false;
}

int CSLSMapData::put(char *key, char *data, int len, int64_t *last_read_time)
{
    int ret = SLS_OK;

    CSLSLock lock(&m_rwclock, true);
    std::string strKey = std::string(key);

    std::map<std::string, CSLSRecycleArray *>::iterator item;
    item = m_map_array.find(strKey);
    if (item == m_map_array.end())
    {
        spdlog::error("[{}] CSLSMapData::put, key={}, not found data array.",
                      fmt::ptr(this), key);
        return SLS_ERROR;
    }
    CSLSRecycleArray *array_data = item->second;
    if (NULL == array_data)
    {
        spdlog::error("[{}] CSLSMapData::get, key={}, array_data is NULL.",
                      fmt::ptr(this), key);
    }

    ret = array_data->put(data, len);
    if (ret != len)
    {
        spdlog::error("[{}] CSLSMapData::put, key={}, array_data->put failed, len={:d}, but ret={:d}.",
                      fmt::ptr(this), key, len, ret);
    }
    if (NULL != last_read_time)
    {
        *last_read_time = array_data->get_last_read_time();
    }

    //check sps and pps
    ts_info *ti = NULL;
    std::map<std::string, ts_info *>::iterator item_ti;
    item_ti = m_map_ts_info.find(strKey);
    if (item_ti == m_map_ts_info.end())
    {
        ti = new ts_info;
        sls_init_ts_info(ti);
        ti->need_spspps = true;
        m_map_ts_info[strKey] = ti;
    }
    else
    {
        ti = item_ti->second;
    }

    if (SLS_OK == check_ts_info(data, len, ti))
    {
        spdlog::info("[{}] CSLSMapData::put, check_spspps ok, key={}.",
                     fmt::ptr(this), key);
    }

    return ret;
}

int CSLSMapData::get(char *key, char *data, int len, SLSRecycleArrayID *read_id, int aligned)
{
    int ret = SLS_OK;

    CSLSLock lock(&m_rwclock, false);
    std::string strKey = std::string(key);

    std::map<std::string, CSLSRecycleArray *>::iterator item;
    item = m_map_array.find(strKey);
    if (item == m_map_array.end())
    {
        spdlog::trace("[{}] CSLSMapData::get, key={}, not found data array,",
                      fmt::ptr(this), key);
        return SLS_ERROR;
    }
    CSLSRecycleArray *array_data = item->second;
    if (NULL == array_data)
    {
        spdlog::warn("[{}] CSLSMapData::get, key={}, array_data is NULL.",
                     fmt::ptr(this), key);
        return SLS_ERROR;
    }

    bool b_first = read_id->bFirst;
    ret = array_data->get(data, len, read_id, aligned);
    if (b_first)
    {
        //get sps and pps
        ret = get_ts_info(key, data, len);
        spdlog::info("[{}] CSLSMapData::get, get sps pps ok, key={}, len={:d}.",
                     fmt::ptr(this), key, ret);
    }
    return ret;
}

int CSLSMapData::get_ts_info(char *key, char *data, int len)
{
    int ret = 0;
    ts_info *ti = NULL;
    std::string strKey = std::string(key);
    std::map<std::string, ts_info *>::iterator item_ti;
    item_ti = m_map_ts_info.find(strKey);
    if (item_ti != m_map_ts_info.end())
    {
        ti = item_ti->second;
        if (len >= TS_UDP_LEN)
        {
            memcpy(data, ti->ts_data, TS_UDP_LEN);
            ret = TS_UDP_LEN;
        }
    }
    return ret;
}

void CSLSMapData::clear()
{
    CSLSLock lock(&m_rwclock, true);
    std::map<std::string, CSLSRecycleArray *>::iterator it;
    for (it = m_map_array.begin(); it != m_map_array.end();)
    {
        CSLSRecycleArray *array_data = it->second;
        if (array_data)
        {
            delete array_data;
        }
        it++;
    }
    m_map_array.clear();
}

int CSLSMapData::check_ts_info(char *data, int len, ts_info *ti)
{
    //only get the first, suppose the sps and pps are not changed always.
    for (int i = 0; i < len;)
    {
        if (ti->sps_len > 0 && ti->pps_len > 0 && ti->pat_len > 0 && ti->pat_len > 0)
        {
            break;
        }
        sls_parse_ts_info((const uint8_t *)data + i, ti);
        i += TS_PACK_LEN;
    }

    return SLS_ERROR;
}
