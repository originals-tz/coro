#ifndef CORO_EVENTFD_H
#define CORO_EVENTFD_H

#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "scheduler.h"
#include "task.h"
#include <list>

namespace coro
{

using fd_t = std::unique_ptr<int, std::function<void(int*)>>;

class EventFdManager
{
public:
    EventFdManager() = default;
    ~EventFdManager()
    {
        for (auto& fd : m_fd_list)
        {
            close(*fd);
            delete fd;
        }
    }

    /**
     * @brief 获取文件描述符
     * @return 文件描述符
     */
    fd_t Acquire()
    {
        std::lock_guard lk(m_mut);
        if (!m_fd_list.empty())
        {
            int* fd = m_fd_list.front();
            m_fd_list.pop_front();
            return {fd, [this](auto ptr) {Release(ptr);}};
        }
        int* fd = new int(eventfd(0, 0));
        return {fd, [this](auto ptr) {Release(ptr);}};
    }

private:
    /**
     * @brief 回收文件描述符
     * @param fd
     */
    void Release(int* fd)
    {
        std::lock_guard lk(m_mut);
        if (*fd == -1)
        {
            return;
        }
        m_fd_list.emplace_back(fd);
    }
    //! 互斥锁
    std::mutex m_mut;
    //! 文件描述符
    std::list<int*> m_fd_list;
};

class EventFdAwaiter : public coro::BaseAwaiter<void>
{
public:
    explicit EventFdAwaiter(int fd)
        : m_event_fd(fd)
    {}

    /**
     * @brief 注册可读事件
     */
    void Handle() override
    {
        auto base = Executor::LocalEventBase();
        m_event = event_new(base, m_event_fd, EV_READ, OnRead, this);
        event_add(m_event, nullptr);
    }

private:
    /**
     * @brief 事件回调
     * @param arg
     */
    static void OnRead(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<EventFdAwaiter*>(arg);
        eventfd_t val = 0;
        eventfd_read(pthis->m_event_fd, &val);
        event_free(pthis->m_event);
        pthis->Resume();
    }

    //! 文件描述符
    int m_event_fd = 0;
    //! 事件
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_EVENTFD_H
