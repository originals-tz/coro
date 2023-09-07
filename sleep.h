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
        m_handler_address = handle.address();
        auto ev = evtimer_new(exec->GetEventBase(), OnTimeout, this);
        evtimer_add(ev, &tv);
        exec->AutoFree(ev);
    }

    void await_resume() {}

private:
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<CoSleep*>(arg);
        auto handle = std::coroutine_handle<TaskPromise>::from_address(pthis->m_handler_address);
        handle.resume();
        if (handle.done())
        {
            handle.promise().Prev();
        }
    }

    int32_t m_duration = 0;
    void* m_handler_address = nullptr;
};

#endif  // COTASK_SLEEP_H