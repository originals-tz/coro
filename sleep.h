#ifndef CORO_SLEEP_H
#define CORO_SLEEP_H

#include "awaiter.h"

namespace coro
{
class Sleep : public coro::BaseAwaiter
{
public:
    explicit Sleep(int sec, int ms = 0);
    ~Sleep() override;
    /**
     * @brief 注册一个超时任务
     */
    void Handle() override;;
private:

    /**
     * @brief 超时后唤醒协程
     * @param arg this指针
     */
    static void OnTimeout(evutil_socket_t, short, void* arg);

    //! 时间
    timeval m_tv;
    //! 超时事件
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_SLEEP_H
