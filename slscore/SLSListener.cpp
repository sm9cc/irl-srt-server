
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

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include "spdlog/spdlog.h"

#include "SLSListener.hpp"
#include "SLSLog.hpp"
#include "SLSPublisher.hpp"
#include "SLSPlayer.hpp"
#include "SLSPullerManager.hpp"
#include "SLSPusherManager.hpp"

const char SLS_SERVER_STAT_INFO_BASE[] = "\
{\
\"port\": \"%d\",\
\"role\": \"%s\",\
\"pub_domain_app\": \"\",\
\"stream_name\": \"\",\
\"url\": \"\",\
\"remote_ip\": \"\",\
\"remote_port\": \"\",\
\"start_time\": \"%s\",\
\"kbitrate\": \"0\"\
}";

const char SLS_PUBLISHER_STAT_INFO_BASE[] = "\
{\
\"port\": \"%d\",\
\"role\": \"%s\",\
\"pub_domain_app\": \"%s\",\
\"stream_name\": \"%s\",\
\"url\": \"%s\",\
\"remote_ip\": \"%s\",\
\"remote_port\": \"%d\",\
\"start_time\": \"%s\",\
\"kbitrate\":\
";

const char SLS_PLAYER_STAT_INFO_BASE[] = "\
{\
\"port\": \"%d\",\
\"role\": \"%s\",\
\"pub_domain_app\": \"%s\",\
\"stream_name\": \"%s\",\
\"url\": \"%s\",\
\"remote_ip\": \"%s\",\
\"remote_port\": \"%d\",\
\"start_time\": \"%s\",\
\"kbitrate\":\
";

/**
 * server conf
 */
SLS_CONF_DYNAMIC_IMPLEMENT(server)

/**
 * CSLSListener class implementation
 */

CSLSListener::CSLSListener()
{
    m_conf = NULL;
    m_back_log = 1024;
    m_is_write = 0;
    m_port = 0;

    m_list_role = NULL;
    m_map_publisher = NULL;
    m_map_puller = NULL;
    m_idle_streams_timeout = UNLIMITED_TIMEOUT;
    m_idle_streams_timeout_role = 0;
    m_stat_info = std::string("");
    memset(m_http_url_role, 0, URL_MAX_LEN);
    memset(m_record_hls_path_prefix, 0, URL_MAX_LEN);

    sprintf(m_role_name, "listener");
}

CSLSListener::~CSLSListener()
{
}

int CSLSListener::init()
{
    int ret = 0;
    return CSLSRole::init();
}

int CSLSListener::uninit()
{
    CSLSLock lock(&m_mutex);
    stop();
    return CSLSRole::uninit();
}

void CSLSListener::set_role_list(CSLSRoleList *list_role)
{
    m_list_role = list_role;
}

void CSLSListener::set_map_publisher(CSLSMapPublisher *publisher)
{
    m_map_publisher = publisher;
}

void CSLSListener::set_map_puller(CSLSMapRelay *map_puller)
{
    m_map_puller = map_puller;
}

void CSLSListener::set_map_pusher(CSLSMapRelay *map_pusher)
{
    m_map_pusher = map_pusher;
}

void CSLSListener::set_record_hls_path_prefix(char *path)
{
    if (path != NULL && strlen(path) > 0)
    {
        strncpy(m_record_hls_path_prefix, path, sizeof(m_record_hls_path_prefix));
    }
}

int CSLSListener::init_conf_app()
{
    string strLive;
    string strUplive;
    string strLiveDomain;
    string strUpliveDomain;
    string strTemp;
    vector<string> domain_players;
    sls_conf_server_t *conf_server;

    if (NULL == m_map_puller)
    {
        spdlog::error("[{}] CSLSListener::init_conf_app failed, m_map_puller is null.", fmt::ptr(this));
        return SLS_ERROR;
    }

    if (NULL == m_map_pusher)
    {
        spdlog::error("[{}] CSLSListener::init_conf_app failed, m_map_pusher is null.", fmt::ptr(this));
        return SLS_ERROR;
    }

    if (!m_conf)
    {
        spdlog::error("[{}] CSLSListener::init_conf_app failed, conf is null.", fmt::ptr(this));
        return SLS_ERROR;
    }
    conf_server = (sls_conf_server_t *)m_conf;

    m_back_log = conf_server->backlog;
    m_idle_streams_timeout_role = conf_server->idle_streams_timeout;
    strncpy(m_http_url_role, conf_server->on_event_url, sizeof(m_http_url_role));
    spdlog::info("[{}] CSLSListener::init_conf_app, m_back_log={:d}, m_idle_streams_timeout={:d}.",
                 fmt::ptr(this), m_back_log, m_idle_streams_timeout_role);

    //domain
    domain_players = sls_conf_string_split(conf_server->domain_player, " ");
    if (domain_players.size() == 0)
    {
        spdlog::error("[{}] CSLSListener::init_conf_app, wrong domain_player='{}'.", fmt::ptr(this), conf_server->domain_player);
        return SLS_ERROR;
    }
    strUpliveDomain = conf_server->domain_publisher;
    if (strUpliveDomain.length() == 0)
    {
        spdlog::error("[{}] CSLSListener::init_conf_app, wrong domain_publisher='{}'.", fmt::ptr(this), conf_server->domain_publisher);
        return SLS_ERROR;
    }
    sls_conf_app_t *conf_app = (sls_conf_app_t *)conf_server->child;
    if (!conf_app)
    {
        spdlog::error("[{}] CSLSListener::init_conf_app, no app conf info.", fmt::ptr(this));
        return SLS_ERROR;
    }

    int app_count = sls_conf_get_conf_count((sls_conf_base_t *)conf_app);
    sls_conf_app_t *ca = conf_app;
    for (int i = 0; i < app_count; i++)
    {
        strUplive = ca->app_publisher;
        if (strUplive.length() == 0)
        {
            spdlog::error("[{}] CSLSListener::init_conf_app, wrong app_publisher='{}', domain_publisher='{}'.",
                          fmt::ptr(this), strUplive.c_str(), strUpliveDomain.c_str());
            return SLS_ERROR;
        }
        strUplive = strUpliveDomain + "/" + strUplive;
        m_map_publisher->set_conf(strUplive, (sls_conf_base_t *)ca);
        spdlog::info("[{}] CSLSListener::init_conf_app, add app push '{}'.",
                     fmt::ptr(this), strUplive.c_str());

        strLive = ca->app_player;
        if (strLive.length() == 0)
        {
            spdlog::error("[{}] CSLSListener::init_conf_app, wrong app_player='{}', domain_publisher='{}'.",
                          fmt::ptr(this), strLive.c_str(), strUpliveDomain.c_str());
            return SLS_ERROR;
        }

        // Setup mapping between player endpoints and publishing endpoints
        // Each player endpoint has a corresponding (not necessarily unique)
        // publishing endpoint.
        for (int j = 0; j < domain_players.size(); j++)
        {
            strLiveDomain = domain_players[j];
            strTemp = strLiveDomain + "/" + strLive;
            if (strUplive == strTemp)
            {
                spdlog::error("[{}] CSLSListener::init_conf_app faild, domain/uplive='{}' and domain/live='{}' must not be equal.",
                              fmt::ptr(this), strUplive.c_str(), strTemp.c_str());
                return SLS_ERROR;
            }
            //m_map_live_2_uplive[strTemp]  = strUplive;
            m_map_publisher->set_live_2_uplive(strTemp, strUplive);

            spdlog::info("[{}] CSLSListener::init_conf_app, add app live='{}', app push='{}'.",
                         fmt::ptr(this), strTemp.c_str(), strUplive.c_str());
        }

        if (NULL != ca->child)
        {
            sls_conf_relay_t *cr = (sls_conf_relay_t *)ca->child;
            while (cr)
            {
                if (strcmp(cr->type, "pull") == 0)
                {
                    if (SLS_OK != m_map_puller->add_relay_conf(strUplive.c_str(), cr))
                    {
                        spdlog::warn("[{}] CSLSListener::init_conf_app, m_map_puller.add_app_conf faile. relay type='{}', app push='{}'.",
                                     fmt::ptr(this), cr->type, strUplive.c_str());
                    }
                }
                else if (strcmp(cr->type, "push") == 0)
                {
                    if (SLS_OK != m_map_pusher->add_relay_conf(strUplive.c_str(), cr))
                    {
                        spdlog::warn("[{}] CSLSListener::init_conf_app, m_map_pusher.add_app_conf faile. relay type='{}', app push='{}'.",
                                     fmt::ptr(this), cr->type, strUplive.c_str());
                    }
                }
                else
                {
                    spdlog::error("[{}] CSLSListener::init_conf_app, wrong relay type='{}', app push='{}'.",
                                  fmt::ptr(this), cr->type, strUplive.c_str());
                    return SLS_ERROR;
                }
                cr = (sls_conf_relay_t *)cr->sibling;
            }
        }

        ca = (sls_conf_app_t *)ca->sibling;
    }
    return SLS_OK;
}

int CSLSListener::start()
{
    int ret = 0;
    std::string strLive;
    std::string strUplive;
    std::string strLiveDomain;
    std::string strUpliveDomain;

    if (NULL == m_conf)
    {
        spdlog::error("[{}] CSLSListener::start failed, conf is null.", fmt::ptr(this));
        return SLS_ERROR;
    }
    spdlog::info("[{}] CSLSListener::start", fmt::ptr(this));

    ret = init_conf_app();
    if (SLS_OK != ret)
    {
        spdlog::error("[{}] CSLSListener::start, init_conf_app failed.", fmt::ptr(this));
        return SLS_ERROR;
    }

    //init listener
    if (NULL == m_srt)
        m_srt = new CSLSSrt();

    int latency = ((sls_conf_server_t *)m_conf)->latency;
    if (latency > 0)
    {
        m_srt->libsrt_set_latency(latency);
    }

    m_port = ((sls_conf_server_t *)m_conf)->listen;
    ret = m_srt->libsrt_setup(m_port);
    if (SLS_OK != ret)
    {
        spdlog::error("[{}] CSLSListener::start, libsrt_setup failure.", fmt::ptr(this));
        return ret;
    }
    spdlog::info("[{}] CSLSListener::start, libsrt_setup ok.", fmt::ptr(this));

    ret = m_srt->libsrt_listen(m_back_log);
    if (SLS_OK != ret)
    {
        spdlog::info("[{}] CSLSListener::start, libsrt_listen failure.", fmt::ptr(this));
        return ret;
    }

    spdlog::info("[{}] CSLSListener::start, m_list_role={}.", fmt::ptr(this), fmt::ptr(m_list_role));
    if (NULL == m_list_role)
    {
        spdlog::info("[{}] CSLSListener::start, m_roleList is null.", fmt::ptr(this));
        return ret;
    }

    spdlog::info("[{}] CSLSListener::start, push to m_list_role={}.", fmt::ptr(this), fmt::ptr(m_list_role));
    m_list_role->push(this);

    return ret;
}

int CSLSListener::stop()
{
    int ret = SLS_OK;
    spdlog::info("[{}] CSLSListener::stop.", fmt::ptr(this));

    return ret;
}

int CSLSListener::handler()
{
    int ret = SLS_OK;
    int fd_client = 0;
    CSLSSrt *srt = NULL;
    char sid[1024] = {0};
    int sid_size = sizeof(sid);
    char host_name[URL_MAX_LEN] = {0};
    char app_name[URL_MAX_LEN] = {0};
    char stream_name[URL_MAX_LEN] = {0};
    char key_app[URL_MAX_LEN] = {0};
    char key_stream_name[URL_MAX_LEN] = {0};
    char tmp[URL_MAX_LEN] = {0};
    char peer_name[IP_MAX_LEN] = {0};
    int peer_port = 0;
    unsigned long peer_addr_raw = 0;
    int client_count = 0;

    //1: accept
    fd_client = m_srt->libsrt_accept();
    if (ret < 0)
    {
        spdlog::error("[{}] CSLSListener::handler, srt_accept failed, fd={:d}.", fmt::ptr(this), get_fd());
        CSLSSrt::libsrt_neterrno();
        return client_count;
    }
    client_count = 1;

    //2.check streamid, split it
    srt = new CSLSSrt;
    srt->libsrt_set_fd(fd_client);
    ret = srt->libsrt_getpeeraddr(peer_name, peer_port);
    if (ret != 0)
    {
        spdlog::error("[{}] CSLSListener::handler, libsrt_getpeeraddr failed, fd={:d}.", fmt::ptr(this), srt->libsrt_get_fd());
        srt->libsrt_close();
        delete srt;
        return client_count;
    }
    spdlog::info("[{}] CSLSListener::handler, new client[{}:{:d}], fd={:d}.", fmt::ptr(this), peer_name, peer_port, fd_client);

    if (0 != srt->libsrt_getsockopt(SRTO_STREAMID, "SRTO_STREAMID", &sid, &sid_size))
    {
        spdlog::error("[{}] CSLSListener::handler, [{}:{:d}], fd={:d}, get streamid info failed.",
                      fmt::ptr(this), peer_name, peer_port, srt->libsrt_get_fd());
        srt->libsrt_close();
        delete srt;
        return client_count;
    }

    if (0 != srt->libsrt_split_sid(sid, host_name, app_name, stream_name))
    {
        spdlog::error("[{}] CSLSListener::handler, [{}:{:d}], parse sid='{}' failed.", fmt::ptr(this), peer_name, peer_port, sid);
        srt->libsrt_close();
        delete srt;
        return client_count;
    }
    spdlog::info("[{}] CSLSListener::handler, [{}:{:d}], sid '{}/{}/{}'",
                 fmt::ptr(this), peer_name, peer_port, host_name, app_name, stream_name);

    // app exist?
    snprintf(key_app, sizeof(key_app), "%s/%s", host_name, app_name);

    std::string app_uplive = "";
    sls_conf_app_t *ca = NULL;

    char cur_time[STR_DATE_TIME_LEN] = {0};
    sls_gettime_default_string(cur_time, sizeof(cur_time));

    //3.is player?
    app_uplive = m_map_publisher->get_uplive(key_app);
    if (app_uplive.length() > 0)
    {
        snprintf(key_stream_name, sizeof(key_stream_name), "%s/%s", app_uplive.c_str(), stream_name);
        CSLSRole *pub = m_map_publisher->get_publisher(key_stream_name);
        if (NULL == pub)
        {
            //*
            //3.1 check pullers
            if (NULL == m_map_puller)
            {
                spdlog::info("[{}] CSLSListener::handler, refused, new role[{}:{:d}], stream='{}', publisher is NULL and m_map_puller is NULL.",
                             fmt::ptr(this), peer_name, peer_port, key_stream_name);
                srt->libsrt_close();
                delete srt;
                return client_count;
            }
            CSLSRelayManager *puller_manager = m_map_puller->add_relay_manager(app_uplive.c_str(), stream_name);
            if (NULL == puller_manager)
            {
                spdlog::info("[{}] CSLSListener::handler, m_map_puller->add_relay_manager failed, new role[{}:{:d}], stream='{}', publisher is NULL, no puller_manager.",
                             fmt::ptr(this), peer_name, peer_port, key_stream_name);
                srt->libsrt_close();
                delete srt;
                return client_count;
            }

            puller_manager->set_map_data(m_map_data);
            puller_manager->set_map_publisher(m_map_publisher);
            puller_manager->set_role_list(m_list_role);
            puller_manager->set_listen_port(m_port);

            if (SLS_OK != puller_manager->start())
            {
                spdlog::info("[{}] CSLSListener::handler, puller_manager->start failed, new client[{}:{:d}], stream='{}'.",
                             fmt::ptr(this), peer_name, peer_port, key_stream_name);
                srt->libsrt_close();
                delete srt;
                return client_count;
            }
            spdlog::info("[{}] CSLSListener::handler, puller_manager->start ok, new client[{}:{:d}], stream={}.",
                         fmt::ptr(this), peer_name, peer_port, key_stream_name);

            pub = m_map_publisher->get_publisher(key_stream_name);
            if (NULL == pub)
            {
                spdlog::info("[{}] CSLSListener::handler, m_map_publisher->get_publisher failed, new client[{}:{:d}], stream={}.",
                             fmt::ptr(this), peer_name, peer_port, key_stream_name);
                srt->libsrt_close();
                delete srt;
                return client_count;
            }
            else
            {
                spdlog::info("[{}] CSLSListener::handler, m_map_publisher->get_publisher ok, pub={}, new client[{}:{:d}], stream={}.",
                             fmt::ptr(this), fmt::ptr(pub), peer_name, peer_port, key_stream_name);
            }
        }

        // Check if IP is allowed to stream from the app
        ca = (sls_conf_app_t *)m_map_publisher->get_ca(app_uplive);
        if (ca == nullptr)
        {
            spdlog::error("[{}] CSLSListener::handler, refused, configuration does not exist [stream={}]",
                          fmt::ptr(this), key_stream_name);
            srt->libsrt_close();
            delete srt;
            return client_count;
        }
        else
        {
            // Get IP address of remote peer
            if (srt->libsrt_getpeeraddr_raw(peer_addr_raw) == SLS_OK)
            {
                // If/when we match an address, set this flag to break out of the for loop
                bool address_matched = false;
                for (sls_ip_access_t &acl_entry : ca->ip_actions.play)
                {
                    if (acl_entry.ip_address == peer_addr_raw || acl_entry.ip_address == 0)
                    {
                        switch (acl_entry.action)
                        {
                        case sls_access_action::ACCEPT:
                            address_matched = true;
                            spdlog::info("[{}] CSLSListener::handler Accepted connection from {}:{:d} for app '{}'",
                                         fmt::ptr(this), peer_name, peer_port, ca->app_publisher);
                            break;
                        case sls_access_action::DENY:
                            spdlog::warn("[{}] CSLSListener::handler Rejected connection from {}:{:d} for app '{}'",
                                         fmt::ptr(this), peer_name, peer_port, ca->app_publisher);
                            srt->libsrt_close();
                            delete srt;
                            return client_count;
                        default:
                            spdlog::error("[{}] CSLSListener::handler Unknown action [sls_access_action={:d}], ignoring",
                                          fmt::ptr(this), (int)acl_entry.action);
                        }
                    }

                    if (address_matched)
                        break;
                }
                // If we don't have any entries regarding the peer, accept by default
                if (!address_matched)
                {
                    spdlog::info("[{}] CSLSListener::handler Accepted connection from {}:{:d} for app '{}' by default",
                                 fmt::ptr(this), peer_name, peer_port, ca->app_publisher);
                }
            }
            else
            {
                spdlog::error("[{}] CSLSListener::handler ACL check failed: could not get peer address", fmt::ptr(this));
                spdlog::error("[{}] CSLSListener::handler Accepting connection by default", fmt::ptr(this));
            }
        }

        //3.2 handle new play
        if (!m_map_data->is_exist(key_stream_name))
        {
            spdlog::error("[{}] CSLSListener::handler, refused, new role[{}:{:d}], stream={}, but publisher data doesn't exist in m_map_data.",
                          fmt::ptr(this), peer_name, peer_port, key_stream_name);
            srt->libsrt_close();
            delete srt;
            return client_count;
        }

        //new player
        if (srt->libsrt_socket_nonblock(0) < 0)
            spdlog::warn("[{}] CSLSListener::handler, new player[{}:{:d}], libsrt_socket_nonblock failed.",
                         fmt::ptr(this), peer_name, peer_port);

        CSLSPlayer *player = new CSLSPlayer;
        player->init();
        player->set_idle_streams_timeout(m_idle_streams_timeout_role);
        player->set_srt(srt);
        player->set_map_data(key_stream_name, m_map_data);
        //stat info
        snprintf(tmp, sizeof(tmp), SLS_PLAYER_STAT_INFO_BASE,
                 m_port, player->get_role_name(), app_uplive.c_str(), stream_name, sid, peer_name, peer_port, cur_time);
        std::string stat_info = std::string(tmp);
        player->set_stat_info_base(stat_info);
        player->set_http_url(m_http_url_role);
        player->on_connect();

        m_list_role->push(player);
        spdlog::info("[{}] CSLSListener::handler, new player[{}] =[{}:{:d}], key_stream_name={}, {}={}, m_list_role->size={:d}.",
                     fmt::ptr(this), fmt::ptr(player), peer_name, peer_port, key_stream_name, pub->get_role_name(), fmt::ptr(pub), m_list_role->size());
        return client_count;
    }

    //4. is publisher?
    app_uplive = key_app;
    snprintf(key_stream_name, sizeof(key_stream_name), "%s/%s", app_uplive.c_str(), stream_name);
    ca = (sls_conf_app_t *)m_map_publisher->get_ca(app_uplive);
    if (NULL == ca)
    {
        spdlog::warn("[{}] CSLSListener::handler, refused, new role[{}:{:d}], non-existent publishing domain [stream='{}']",
                     fmt::ptr(this), peer_name, peer_port, key_stream_name);
        srt->libsrt_close();
        delete srt;
        return client_count;
    }

    // Check if IP is allowed to publish to the app
    if (srt->libsrt_getpeeraddr_raw(peer_addr_raw) == SLS_OK)
    {
        // If/when we match an address, set this flag to break out of the for loop
        bool address_matched = false;
        for (sls_ip_access_t &acl_entry : ca->ip_actions.publish)
        {
            if (acl_entry.ip_address == peer_addr_raw || acl_entry.ip_address == 0)
            {
                switch (acl_entry.action)
                {
                case sls_access_action::ACCEPT:
                    address_matched = true;
                    spdlog::info("[{}] CSLSListener::handler Accepted connection from {}:{:d} for app '{}'",
                                 fmt::ptr(this), peer_name, peer_port, ca->app_publisher);
                    break;
                case sls_access_action::DENY:
                    spdlog::warn("[{}] CSLSListener::handler Rejected connection from {}:{:d} for app '{}'",
                                 fmt::ptr(this), peer_name, peer_port, ca->app_publisher);
                    srt->libsrt_close();
                    delete srt;
                    return client_count;
                default:
                    spdlog::error("[{}] CSLSListener::handler Unknown action [sls_access_action={:d}], ignoring",
                                  fmt::ptr(this), (int)acl_entry.action);
                }
            }

            if (address_matched)
                break;
        }
        // If we don't have any entries regarding the peer, accept by default
        if (!address_matched)
        {
            spdlog::info("[{}] CSLSListener::handler Accepted connection from {}:{:d} for app '{}' by default",
                         fmt::ptr(this), peer_name, peer_port, ca->app_publisher);
        }
    }
    else
    {
        spdlog::error("[{}] CSLSListener::handler ACL check failed: could not get peer address", fmt::ptr(this));
        spdlog::error("[{}] CSLSListener::handler Accepting connection by default", fmt::ptr(this));
    }

    // Check if publisher for the stream already exists
    CSLSRole *publisher = m_map_publisher->get_publisher(key_stream_name);
    if (NULL != publisher)
    {
        spdlog::error("[{}] CSLSListener::handler, refused, new role[{}:{:d}], stream='{}',but publisher={} is not NULL.",
                      fmt::ptr(this), peer_name, peer_port, key_stream_name, fmt::ptr(publisher));
        srt->libsrt_close();
        delete srt;
        return client_count;
    }
    //create new publisher
    CSLSPublisher *pub = new CSLSPublisher;
    pub->set_srt(srt);
    pub->set_conf((sls_conf_base_t *)ca);
    pub->init();
    pub->set_idle_streams_timeout(m_idle_streams_timeout_role);
    //stat info
    snprintf(tmp, sizeof(tmp), SLS_PUBLISHER_STAT_INFO_BASE,
             m_port, pub->get_role_name(), app_uplive.c_str(), stream_name, sid, peer_name, peer_port, cur_time);
    std::string stat_info = std::string(tmp);
    pub->set_stat_info_base(stat_info);
    pub->set_http_url(m_http_url_role);
    //set hls record path
    snprintf(tmp, sizeof(tmp), "%s/%d/%s",
             m_record_hls_path_prefix, m_port, key_stream_name);
    pub->set_record_hls_path(tmp);

    spdlog::info("[{}] CSLSListener::handler, new pub={}, key_stream_name={}.",
                 fmt::ptr(this), fmt::ptr(pub), key_stream_name);

    //init data array
    if (SLS_OK != m_map_data->add(key_stream_name))
    {
        spdlog::warn("[{}] CSLSListener::handler, m_map_data->add failed, new pub[{}:{:d}], stream={}.",
                     fmt::ptr(this), peer_name, peer_port, key_stream_name);
        pub->uninit();
        delete pub;
        pub = NULL;
        return client_count;
    }

    if (SLS_OK != m_map_publisher->set_push_2_pushlisher(key_stream_name, pub))
    {
        spdlog::warn("[{}] CSLSListener::handler, m_map_publisher->set_push_2_pushlisher failed, key_stream_name={}.",
                     fmt::ptr(this), key_stream_name);
        pub->uninit();
        delete pub;
        pub = NULL;
        return client_count;
    }
    pub->set_map_publisher(m_map_publisher);
    pub->set_map_data(key_stream_name, m_map_data);
    pub->on_connect();
    m_list_role->push(pub);
    spdlog::info("[{}] CSLSListener::handler, new publisher[{}:{:d}], key_stream_name={}.",
                 fmt::ptr(this), peer_name, peer_port, key_stream_name);

    //5. check pusher
    if (NULL == m_map_pusher)
    {
        return client_count;
    }
    CSLSRelayManager *pusher_manager = m_map_pusher->add_relay_manager(app_uplive.c_str(), stream_name);
    if (NULL == pusher_manager)
    {
        spdlog::info("[{}] CSLSListener::handler, m_map_pusher->add_relay_manager failed, new role[{}:{:d}], key_stream_name={}.",
                     fmt::ptr(this), peer_name, peer_port, key_stream_name);
        return client_count;
    }
    pusher_manager->set_map_data(m_map_data);
    pusher_manager->set_map_publisher(m_map_publisher);
    pusher_manager->set_role_list(m_list_role);
    pusher_manager->set_listen_port(m_port);

    if (SLS_OK != pusher_manager->start())
    {
        spdlog::info("[{}] CSLSListener::handler, pusher_manager->start failed, new role[{}:{:d}], key_stream_name={}.",
                     fmt::ptr(this), peer_name, peer_port, key_stream_name);
    }
    return client_count;
}

std::string CSLSListener::get_stat_info()
{
    if (m_stat_info.length() == 0)
    {
        char tmp[STR_MAX_LEN] = {0};
        char cur_time[STR_DATE_TIME_LEN] = {0};
        sls_gettime_default_string(cur_time, sizeof(cur_time));
        snprintf(tmp, sizeof(tmp), SLS_SERVER_STAT_INFO_BASE, m_port, m_role_name, cur_time);
        m_stat_info = std::string(tmp);
    }
    return m_stat_info;
}
