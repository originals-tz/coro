#ifndef CORO_CHANNEL_H
#define CORO_CHANNEL_H

#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "scheduler.h"
#include "task.h"

namespace coro
{
enum E_CHAN_STATUS
{
    e_chan_success, //成功
    e_chan_close    //关闭
};

class ChannelAwait :  public coro::BaseAwaiter<void>
{
public:
    explicit ChannelAwait(int32_t fd)
        : m_evfd(fd)
    {
    }

    /**
     * @brief 注册事件
     */
    void Handle() override
    {
        auto base = Executor::LocalEventBase();
        m_event = event_new(base, m_evfd, EV_READ, OnRead, this);
        event_add(m_event, nullptr);
    }

private:
    /**
     * @brief 回调
     * @param arg
     */
    static void OnRead(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<ChannelAwait*>(arg);
        eventfd_t val = 0;
        eventfd_read(pthis->m_evfd, &val);
        event_free(pthis->m_event);
        pthis->Resume();
    }

    //! eventfd
    int m_evfd = 0;
    //! 事件
    event* m_event = nullptr;
};

template<typename T>
class Channel
{
public:
    Channel() = default;
    ~Channel()
    {
        while(!m_fd_queue.empty())
        {
            close(m_fd_queue.front());
            m_fd_queue.pop();
        }
    }

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
        for (auto& fd : m_waiting_list)
        {
            eventfd_write(fd, val);
        }
    }

    /**
     * @brief 添加一个数据
     * @param t
     * @return
     */
    E_CHAN_STATUS Push(T&& t)
    {
        if (m_is_close)
        {
            return e_chan_close;
        }
        m_data_queue.push(std::move(t));
        Notify();
        return e_chan_success;
    }

    /***
     * @brief 添加一个数据
     * @param t
     * @return
     */
    E_CHAN_STATUS Push(const T& t)
    {
        if (m_is_close)
        {
            return e_chan_close;
        }
        m_data_queue.push(t);
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

        int32_t fd = 0;
        E_CHAN_STATUS s = e_chan_close;
        do {
            {
                std::lock_guard lk(m_mut);
                if (!m_data_queue.empty())
                {
                    t = std::move(m_data_queue.front().value());
                    m_data_queue.pop();
                    s = e_chan_success;
                    break;
                }
                if (fd == 0)
                {
                    fd = GetEventFD();
                }
                m_waiting_list.emplace(fd);
            }
            co_await ChannelAwait(fd);
        } while(!m_is_close);

        ReleaseFD(fd);
        co_return s;
    }
private:
    /**
     * @brief 通知协程
     */
    void Notify()
    {
        std::lock_guard lk(m_mut);
        if (!m_waiting_list.empty())
        {
            int fd = *m_waiting_list.begin();
            eventfd_t val = 1;
            eventfd_write(fd, val);
        }
    }

    /**
     * @brief 获取fd
     * @return
     */
    int GetEventFD()
    {
        std::lock_guard lk(m_mut);
        if (!m_fd_queue.empty())
        {
            int fd = m_fd_queue.front();
            m_fd_queue.pop();
            return fd;
        }
        return eventfd(0, 0);
    }

    /**
     * @brief 回收fd
     * @param fd
     */
    void ReleaseFD(int fd)
    {
        std::lock_guard lk(m_mut);
        m_fd_queue.emplace(fd);
    }

    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! 数据队列
    std::queue<std::optional<T>> m_data_queue;
    //! fd队列
    std::queue<int32_t> m_fd_queue;
    //! 正在等待的fd列表
    std::set<int> m_waiting_list;
    //! 是否关闭
    std::atomic_bool m_is_close = false;
};
}

#endif  // CORO_CHANNEL_H
