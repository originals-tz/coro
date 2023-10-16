#ifndef COTASK_DB_H
#define COTASK_DB_H
#include <mysql/mysql.h>

class DB
{
public:
    DB()
    {
        m_db = mysql_init(nullptr);
    }

    ~DB()
    {
        mysql_close(m_db);
    }

    Task Connect(std::string_view ip, int32_t port, std::string_view username, std::string_view pass, std::string_view default_db)
    {
        // 运行状态
        net_async_status s = NET_ASYNC_COMPLETE;
        // 标记
        bool iscontinue = true;
        // 最大重试次数, 仅限于网络错误
        int32_t maxretry = 3;
        // 重试次数, 仅限于网络错误
        int32_t retry = 0;
        uint32_t err = 0;
        // 设置默认编码
        // 设置自动重连
        int32_t check_count = 0;
        while(iscontinue)
        {
            check_count++;
            s  = mysql_real_connect_nonblocking(m_db, ip.data(), username.data(), pass.data(), default_db.data(), port, nullptr, 0);
            switch (s)
            {
                case NET_ASYNC_COMPLETE:
                {
                    iscontinue = false;
                    break;
                }
                case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:  // connect 不会触发这个
                {
                    iscontinue = false;
                    break;
                }
                case NET_ASYNC_NOT_READY:
                {
                    // 设置本次等待的信号
                    co_await CoSleep(0, 10000);
                    break;
                }
                case NET_ASYNC_ERROR:
                {
                    err = mysql_errno(m_db);
                    if (err != 2000)
                    {
                        // 其他错误直接退出
                        iscontinue = false;
                        break;
                    }
                }
                default:
                {
                    // 网络错误， 做最大努力的重试，重试次数为 maxretry
                    iscontinue = (retry++ < maxretry);
                    if (iscontinue)
                    {
                        std::cout << "发生网络错误" << std::endl;
                    }
                    break;
                }
            }
        }
        std::cout << "协程切换次数" << check_count << std::endl;
        if (s != NET_ASYNC_COMPLETE)
        {
            std::cout <<  "登陆失败" << std::endl;
        }
        co_return;
    }

    Task Query(const std::string& sql)
    {
        // 运行状态
        net_async_status s = NET_ASYNC_COMPLETE;
        // 标记
        bool iscontinue = true;
        int32_t retry = 0;
        uint32_t err = 0;
        int32_t check_count = 0;
        while (iscontinue)
        {
            check_count++;
            s = mysql_real_query_nonblocking(m_db, sql.c_str(), sql.size());
            switch (s)
            {
                case NET_ASYNC_COMPLETE:
                    iscontinue = false; // 本次异步操作已经完成, 通过mysql官方api描述，本操作不能作为query结果，必须要结合 mysql_errno 结合判断
                    break;
                case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:
                    iscontinue = false;// 表明当前db查询有更多的结果集
                    break;
                case NET_ASYNC_NOT_READY:
                {
                    co_await CoSleep(0, 10000);
                }
                break;
                case NET_ASYNC_ERROR:
                {
                    err = mysql_errno(m_db);
                    //1158:网络错误,出现读错误,请检查网络连接状况
                    //1159:网络错误,读超时,请检查网络连接状况
                    //1160:网络错误,出现写错误,请检查网络连接状况
                    //1161:网络错误,写超时,请检查网络连接状况
                    if (err < 1158 || err > 1161)
                    {
                        // 其他错误直接退出
                        iscontinue = false;
                        break;
                    }
                }
                default:
                    // 网络错误， 做最大努力的重试，重试次数为 maxretry
                    iscontinue = (retry++ < 3);
                    // 通过ping，触发 mysql 自身的重连处理机制, 试图修正一些网络不稳定的情况
                    break;
            }
        }
        std::cout << "协程切换次数" << check_count << std::endl;
        if (s != NET_ASYNC_COMPLETE && s != NET_ASYNC_COMPLETE_NO_MORE_RESULTS)
        {
            std::cout << "查询错误" << std::endl;
        }
        co_return;
    }

    Task CoStoreResult()
    {
        // 运行状态
        net_async_status s = NET_ASYNC_COMPLETE;
        // 标记
        bool iscontinue = true;
        MYSQL_RES* m_res;
        int32_t check_count = 0;
        while (iscontinue)
        {
            check_count++;
            s = mysql_store_result_nonblocking(m_db, &m_res);
            switch (s)
            {
                case NET_ASYNC_COMPLETE:
                case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:
                {
                    iscontinue = false;
                }
                break;
                case NET_ASYNC_NOT_READY:
                {
                    co_await CoSleep(0, 10000);
                }
                break;
                case NET_ASYNC_ERROR:
                default:
                    iscontinue = false;
                    break;
            }
        }
        std::cout << "协程切换次数" << check_count << std::endl;
        co_return ;
    }
private:
    //! MYSql 句柄
    MYSQL* m_db;
};

#endif  // COTASK_DB_H
