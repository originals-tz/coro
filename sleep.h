#ifndef CORO_SLEEP_H
#define CORO_SLEEP_H

#include "awaiter.h"
#include "scheduler.h"

namespace coro
{
class Sleep : public coro::BaseAwaiter<void>
{
public:
    Sleep(int sec, int ms = 0)
        : m_sec(sec)
        , m_usec(ms * 1000)
    {}

    void Handle() override
    {
        timeval tv{.tv_sec = m_sec, .tv_usec = m_usec};
        m_event = evtimer_new(Executor::LocalEventBase(), OnTimeout, this);
        evtimer_add(m_event, &tv);
    }

private:
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<Sleep*>(arg);
        evtimer_del(pthis->m_event);
        event_free(pthis->m_event);
        pthis->Resume();
    }

    int m_sec = 0;
    int m_usec = 0;
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_SLEEP_H
