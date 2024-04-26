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

class EventfdAwaiter : public coro::BaseAwaiter<void>
{
public:
    explicit EventfdAwaiter(int32_t fd)
        : m_evfd(fd)
    {}

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
        auto pthis = static_cast<EventfdAwaiter*>(arg);
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

}  // namespace coro

#endif  // CORO_EVENTFD_H
