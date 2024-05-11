#ifndef CORO_EVENTFD_H
#define CORO_EVENTFD_H

#include <sys/eventfd.h>
#include <list>
#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "executor.h"
#include "task.h"

namespace coro
{
class EventFdAwaiter : public coro::BaseAwaiter<void>
{
public:
    explicit EventFdAwaiter(int fd)
        : m_event_fd(fd)
    {}

    ~EventFdAwaiter() override
    {
        if (m_event)
        {
            event_free(m_event);
        }
    }

    /**
     * @brief 注册fd可读事件
     */
    void Handle() override
    {
        if (!m_event)
        {
            auto base = Executor::LocalEventBase();
            m_event = event_new(base, m_event_fd, EV_READ, OnRead, this);
        }
        event_add(m_event, nullptr);
    }

private:
    /**
     * @brief fd可读回调
     * @param arg 传递this指针
     */
    static void OnRead(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<EventFdAwaiter*>(arg);
        eventfd_t val = 0;
        eventfd_read(pthis->m_event_fd, &val);
        pthis->Resume();
    }

    //! 文件描述符
    int m_event_fd = 0;
    //! 事件
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_EVENTFD_H
