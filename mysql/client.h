#ifndef CORO_MYSQLCLIENT_H
#define CORO_MYSQLCLIENT_H

#include <mysql/mysql.h>
#include "../sleep.h"
#include "../task.h"

namespace coro
{
class MYSQLClient
{
public:
    MYSQLClient() = default;
    ~MYSQLClient();

    /**
     * @brief 同步连接mysql
     * @param host
     * @param name
     * @param passwd
     * @param database
     * @param port
     * @param timeout
     * @return
     */
    uint32_t Connect(const std::string& host, const std::string& name, const std::string& passwd, const std::string& database, int32_t port, int32_t timeout = 12);

    /**
     * @brief 连接mysql
     * @param host 数据库地址
     * @param name 用户名
     * @param passwd 密码
     * @param database 数据库
     * @param port 端口
     * @param timeout 超时时间
     * @return 状态，错误码
     */
    Task<std::pair<net_async_status, uint32_t>> AsyncConnect(const std::string& host, const std::string& name, const std::string& passwd, const std::string& database, int32_t port, int32_t timeout = 12);

    /**
     * @brief ping
     * @return 是否成功，错误码
     */
    std::pair<bool, uint32_t> Ping() const;

    /**
     * @brief 查询sql
     * @param sql
     * @return 状态，错误码
     */
    Task<std::pair<net_async_status, uint32_t>> Query(const std::string& sql);

    /**
     * @brief 存储数据 mysql_store_result_nonblocking
     * @return 结果,错误码
     */
    Task<std::pair<MYSQL_RES*, int32_t>> StoreResult();

    /**
     * @brief 对标mysql_fetch_row_nonblocking
     * @param res
     * @return 结果，错误码
     */
    Task<std::pair<MYSQL_ROW, int32_t>> FetchRow(MYSQL_RES* res);

    /**
     * @brief 获取db
     * @return
     */
    MYSQL* GetCon();

private:
    //! mysql db
    MYSQL* m_con = nullptr;
    //! 超时
    int32_t m_connect_timeout = 0;
    //! 端口
    int32_t m_port = 0;
    //! ip
    std::string m_host;
    //! 用户名
    std::string m_name;
    //! 密码
    std::string m_passwd;
    //! 数据库
    std::string m_database;
};

}  // namespace coro

#endif  // CORO_MYSQLCLIENT_H
