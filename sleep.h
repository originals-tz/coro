#ifndef CORO_SLEEP_H
#define CORO_SLEEP_H

#include "awaiter.h"
#include "executor.h"

namespace coro
{
class Sleep : public coro::BaseAwaiter<void>
{
public:
    explicit Sleep(int sec, int ms = 0)
        : m_sec(sec)
        , m_usec(ms * 1000)
    {}

    /**
     * @brief 注册一个超时任务
     */
    void Handle() override
    {
        timeval tv{.tv_sec = m_sec, .tv_usec = m_usec};
        m_event = evtimer_new(Executor::LocalEventBase(), OnTimeout, this);
        evtimer_add(m_event, &tv);
    }

private:
    /**
     * @brief 超时后唤醒协程
     * @param arg this指针
     */
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<Sleep*>(arg);
        event_free(pthis->m_event);
        pthis->Resume();
    }

    //! 超时秒数
    int m_sec = 0;
    //! 超时微秒
    int m_usec = 0;
    //! 超时事件
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_SLEEP_H
