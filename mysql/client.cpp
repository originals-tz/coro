#include "client.h"

namespace coro
{
MYSQLClient::~MYSQLClient()
{
    if (m_con)
    {
        mysql_close(m_con);
    }
}

uint32_t MYSQLClient::Connect(const std::string& host, const std::string& name, const std::string& passwd, const std::string& database, int32_t port, int32_t timeout)
{
    if (m_con)
    {
        mysql_close(m_con);
    }
    m_port = port;
    m_host = host;
    m_name = name;
    m_passwd = passwd;
    m_database = database;
    m_connect_timeout = timeout;

    uint32_t err = 0;
    int32_t reconnect = 0;

    m_con = mysql_init(nullptr);
    mysql_options(m_con, MYSQL_SET_CHARSET_NAME, "utf8");
    mysql_options(m_con, MYSQL_INIT_COMMAND, "SET NAMES utf8");
    mysql_options(m_con, MYSQL_OPT_CONNECT_TIMEOUT, &m_connect_timeout);
    mysql_options(m_con, MYSQL_OPT_COMPRESS, nullptr);

    MYSQL* res = nullptr;
    while(!(res = mysql_real_connect(m_con, host.c_str(), name.c_str(), passwd.c_str(), database.c_str(), port, nullptr, 0)) &&
           reconnect++ < 3);
    if (!res)
    {
        err = mysql_errno(m_con);
    }
    return err;
}

Task<std::pair<net_async_status, uint32_t>> MYSQLClient::AsyncConnect(const std::string& host, const std::string& name, const std::string& passwd, const std::string& database, int32_t port, int32_t timeout)
{
    if (m_con)
    {
        mysql_close(m_con);
    }

    m_port = port;
    m_host = host;
    m_name = name;
    m_passwd = passwd;
    m_database = database;

    uint32_t err = 0;
    m_connect_timeout = timeout;

    int32_t reconnect = 0;
    bool is_continue = true;
    net_async_status s = NET_ASYNC_COMPLETE;

    m_con = mysql_init(nullptr);
    mysql_options(m_con, MYSQL_SET_CHARSET_NAME, "utf8");
    mysql_options(m_con, MYSQL_INIT_COMMAND, "SET NAMES utf8");
    mysql_options(m_con, MYSQL_OPT_CONNECT_TIMEOUT, &m_connect_timeout);
    mysql_options(m_con, MYSQL_OPT_COMPRESS, nullptr);
    while (is_continue)
    {
        s = mysql_real_connect_nonblocking(m_con, host.c_str(), name.c_str(), passwd.c_str(), database.c_str(), port, nullptr, 0);
        if (s == NET_ASYNC_COMPLETE)
        {
            break;
        }
        else if (s == NET_ASYNC_NOT_READY)
        {
            co_await Sleep(0, 16);
        }
        else if (s == NET_ASYNC_ERROR)
        {
            std::string errstr = mysql_error(m_con);
            err = mysql_errno(m_con);
            if (err != 2000)
            {
                is_continue = false;
            }
            else
            {
                is_continue = (reconnect++ < 3);
            }
        }
    }

    if (s != NET_ASYNC_COMPLETE)
    {
        err = mysql_errno(m_con);
        mysql_close(m_con);
        m_con = nullptr;
    }
    co_return {s, err};
}


std::pair<bool, uint32_t> MYSQLClient::Ping() const
{
    if (!m_con)
    {
        return {false, 0};
    }

    if (mysql_ping(m_con) != 0)
    {
        return {false, mysql_errno(m_con)};
    }
    return {true, 0};
}

Task<std::pair<net_async_status, uint32_t>> MYSQLClient::Query(const std::string& sql)
{
    if (!m_con)
    {
        co_return {NET_ASYNC_ERROR, 0};
    }

    uint32_t err = 0;
    int32_t re_try = 0;
    bool is_continue = true;
    net_async_status s = NET_ASYNC_COMPLETE;

    while (is_continue && re_try < 3)
    {
        s = mysql_real_query_nonblocking(m_con, sql.c_str(), sql.size());
        if (s == NET_ASYNC_COMPLETE)
        {
            break;
        }
        else if (s == NET_ASYNC_NOT_READY)
        {
            co_await Sleep(0, 16);
        }
        else if (s == NET_ASYNC_ERROR)
        {
            err = mysql_errno(m_con);
            if (err == 2003 ||
                err == 2006 ||
                err == 2013 ||
                err == 2055)
            {
                auto [tmp_s, tmp_err] = co_await AsyncConnect(m_host, m_name, m_passwd, m_database, m_port);
                if (tmp_err != 0)
                {
                    s = tmp_s;
                    err = tmp_err;
                    is_continue = false;
                }
                else
                {
                    re_try++;
                }
                continue;
            }
            is_continue = false;
        }
    }
    co_return {s, err};
}

Task<std::pair<MYSQL_RES*, int32_t>> MYSQLClient::StoreResult()
{
    if (!m_con)
    {
        co_return {nullptr, 0};
    }

    net_async_status s = NET_ASYNC_COMPLETE;
    MYSQL_RES* result = nullptr;
    do {
        s = mysql_store_result_nonblocking(m_con, &result);
        if (s == NET_ASYNC_NOT_READY)
        {
            co_await Sleep(0, 16);
        }
    } while(s == NET_ASYNC_NOT_READY);

    if (s == NET_ASYNC_ERROR || !result)
    {
        co_return {nullptr, mysql_errno(m_con)};
    }
    co_return {result, 0};
}

Task<std::pair<MYSQL_ROW, int32_t>> MYSQLClient::FetchRow(MYSQL_RES* res)
{
    if (!res)
    {
        co_return {nullptr, 0};
    }

    net_async_status s = NET_ASYNC_COMPLETE;
    MYSQL_ROW row = nullptr;
    do {
        s = mysql_fetch_row_nonblocking(res, &row);
        if (s == NET_ASYNC_NOT_READY)
        {
            co_await Sleep(0, 16);
        }
    } while(s == NET_ASYNC_NOT_READY);

    if (s == NET_ASYNC_ERROR || !row)
    {
        co_return {nullptr, mysql_errno(m_con)};
    }
    co_return {row, 0};
}

MYSQL* MYSQLClient::GetCon() { return m_con; }

}
