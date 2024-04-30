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
    /**
     * @brief 关闭channel，唤醒所有协程
     */
    void Close()
    {
        if (m_is_close)
        {
            return;
        }

        std::lock_guard lk(m_mut);
        m_is_close = true;
        eventfd_t val = 1;
        for (auto& fd : m_waiting_fd)
        {
            eventfd_write(fd, val);
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
        Notify();
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

        fd_t fd;
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

                if (!fd)
                {
                    fd = m_fd_mgr.Acquire();
                    m_waiting_fd.emplace(*fd);
                }
            }
            co_await EventFdAwaiter(*fd);
        } while (!m_is_close);
        m_waiting_fd.erase(*fd);
        co_return s;
    }

private:
    /**
     * @brief 通知协程
     */
    void Notify()
    {
        std::lock_guard lk(m_mut);
        if (!m_waiting_fd.empty())
        {
            int fd = *m_waiting_fd.begin();
            eventfd_t val = 1;
            eventfd_write(fd, val);
        }
    }

    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! 数据队列
    std::queue<std::optional<T>> m_data_queue;
    //! 正在等待的fd列表
    std::set<int> m_waiting_fd;
    //! 是否关闭
    std::atomic_bool m_is_close = false;
    //! 文件描述符管理
    EventFdManager m_fd_mgr;
};
}  // namespace coro

#endif  // CORO_CHANNEL_H
