#ifndef CORO_EVENTFD_H
#define CORO_EVENTFD_H

#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "scheduler.h"
#include "task.h"

namespace coro
{

class EventFdAwaiter : public coro::BaseAwaiter<void>
{
public:
    explicit EventFdAwaiter(int32_t fd)
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
