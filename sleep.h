#ifndef CORO_SLEEP_H
#define CORO_SLEEP_H

#include "awaiter.h"
#include "executor.h"
#include "task.h"
#include "event2/event.h"

namespace coro
{
class Sleep : public coro::BaseAwaiter<void>
{
public:
    explicit Sleep(int sec, int ms = 0)
        : m_tv({.tv_sec = sec, .tv_usec = ms * 1000})
    {
    }

    ~Sleep() override
    {
        if (m_event)
        {
            event_free(m_event);
        }
    }

    /**
     * @brief 注册一个超时任务
     */
    void Handle() override
    {
        if (!m_event)
        {
            m_event = evtimer_new(EventBase(), OnTimeout, this);
        }
        evtimer_add(m_event, &m_tv);
    }

private:
    /**
     * @brief 超时后唤醒协程
     * @param arg this指针
     */
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<Sleep*>(arg);
        pthis->Resume();
    }

    timeval m_tv;
    //! 超时事件
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_SLEEP_H
