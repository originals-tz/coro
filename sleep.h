#ifndef COTASK_SLEEP_H
#define COTASK_SLEEP_H

#include "awaiter.h"
#include "scheduler.h"

class CoSleep : public BaseAwaiter
{
public:
    CoSleep(int32_t duration)
        : m_duration(duration)
    {}

    ~CoSleep() {}

    void Handle() override
    {
        auto exec = Executor::ThreadLocalInstance();
        timeval tv{.tv_sec = m_duration, .tv_usec = 0};
        auto ev = evtimer_new(exec->GetEventBase(), OnTimeout, this);
        evtimer_add(ev, &tv);
        exec->AutoFree(ev);
    }
private:
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<CoSleep*>(arg);
        pthis->Resume();
    }
    int32_t m_duration = 0;
};

#endif  // COTASK_SLEEP_H