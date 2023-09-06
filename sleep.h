#ifndef COTASK_SLEEP_H
#define COTASK_SLEEP_H

#include "scheduler.h"
class CoSleep
{
public:
    CoSleep(int32_t duration)
        : m_duration(duration)
    {
    }

    bool await_ready() const { return false;}

    void await_suspend(std::coroutine_handle<TaskPromise> handle)
    {
        timeval tv{
            .tv_sec = m_duration,
            .tv_usec = 0
        };
        m_handle = handle;
        m_ev = evtimer_new(Worker::EventBase(), OnTimeout, this);
        evtimer_add(m_ev, &tv);
    }

    void await_resume() {}
private:
    static void OnTimeout(evutil_socket_t, short, void * arg)
    {
        auto pthis = static_cast<CoSleep*>(arg);
        pthis->m_handle.resume();
        evtimer_del(pthis->m_ev);
        event_free(pthis->m_ev);
    }

    int32_t m_duration = 0;
    event* m_ev = nullptr;
    std::coroutine_handle<TaskPromise> m_handle;
};

#endif //COTASK_SLEEP_H