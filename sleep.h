#ifndef COTASK_SLEEP_H
#define COTASK_SLEEP_H

#include "scheduler.h"

class CoSleep
{
public:
    CoSleep(int32_t duration)
        : m_duration(duration)
    {}

    ~CoSleep() {}

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<TaskPromise> handle)
    {
        auto exec = Executor::ThreadLocalInstance();
        assert(exec);
        timeval tv{.tv_sec = m_duration, .tv_usec = 0};
        m_handle = handle;
        auto ev = evtimer_new(exec->GetEventBase(), OnTimeout, this);
        evtimer_add(ev, &tv);
        exec->AutoFree(ev);
    }

    void await_resume() {}

private:
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<CoSleep*>(arg);
        Executor::ResumeCoroutine(pthis->m_handle);
    }

    int32_t m_duration = 0;
    std::coroutine_handle<TaskPromise> m_handle;
};

#endif  // COTASK_SLEEP_H