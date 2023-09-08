#ifndef COTASK_SLEEP_H
#define COTASK_SLEEP_H

#include "awaiter.h"
#include "scheduler.h"

class CoSleep : public BaseAwaiter
{
public:
    CoSleep(int32_t sec, int32_t usec = 0)
        : m_sec(sec)
        , m_usec(usec)
    {}

    ~CoSleep() {}

    void Handle() override
    {
        auto exec = Executor::ThreadLocalInstance();
        timeval tv{.tv_sec = m_sec, .tv_usec = m_usec};
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
    int32_t m_sec = 0;
    int32_t m_usec = 0;
};

#endif  // COTASK_SLEEP_H