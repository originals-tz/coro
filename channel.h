#ifndef CORO_CHANNEL_H
#define CORO_CHANNEL_H

#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "eventfd.h"
#include "scheduler.h"
#include "task.h"

namespace coro
{
enum E_CHAN_STATUS {
    e_chan_success,  // 成功
    e_chan_close     // 关闭
};

template <typename T>
class Channel
{
public:
    Channel()
    {
        m_fd = eventfd(0, 0);
    }

    ~Channel()
    {
        close(m_fd);
    }

    /**
     * @brief 关闭channel，唤醒所有协程
     */
    void Close()
    {
        if (!m_is_close)
        {
            m_is_close = true;
            eventfd_write(m_fd, 1);
        }
    }

    /**
     * @brief 添加一个数据
     * @param t 数据
     * @return 是否成功添加
     */
    template <class DATA>
    E_CHAN_STATUS Push(DATA&& t)
    {
        if (m_is_close)
        {
            return e_chan_close;
        }
        std::lock_guard lk(m_mut);
        m_data_queue.emplace(std::forward<DATA>(t));
        eventfd_write(m_fd, 1);
        return e_chan_success;
    }

    /**
     * @brief 获取一个数据
     * @param t
     * @return
     */
    Task<E_CHAN_STATUS> Pop(T& t)
    {
        if (m_is_close)
        {
            co_return e_chan_close;
        }

        E_CHAN_STATUS s = e_chan_close;
        do
        {
            {
                std::lock_guard lk(m_mut);
                if (!m_data_queue.empty())
                {
                    t = std::move(m_data_queue.front().value());
                    m_data_queue.pop();
                    s = e_chan_success;
                    break;
                }
            }
            co_await EventFdAwaiter(m_fd);
        } while (!m_is_close);
        //! 唤醒其他等待的协程退出
        eventfd_write(m_fd, 1);
        co_return s;
    }

    /***
     * @brief 获取数据
     * @param t
     * @return
     */
    Task<E_CHAN_STATUS> operator>>(T& t)
    {
        co_return Pop(t);
    }

private:
    //! eventfd
    int m_fd = 0;
    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! 数据队列
    std::queue<std::optional<T>> m_data_queue;
    //! 是否关闭
    std::atomic_bool m_is_close = false;
    //! 文件描述符管理
    EventFdManager m_fd_mgr;
};
}  // namespace coro

#endif  // CORO_CHANNEL_H
