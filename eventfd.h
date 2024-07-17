#ifndef CORO_EVENTFD_H
#define CORO_EVENTFD_H

#include "awaiter.h"

namespace coro
{
class EventFdAwaiter : public coro::BaseAwaiter
{
public:
    explicit EventFdAwaiter(int fd);
    ~EventFdAwaiter() override;

    /**
     * @brief 注册fd可读事件
     */
    void Handle() override;
private:
    /**
     * @brief event fd可读回调
     * @param arg this指针
     */
    static void OnRead(evutil_socket_t, short, void* arg);

    //! event fd
    int m_event_fd = 0;
    //! 事件
    event* m_event = nullptr;
};

}  // namespace coro

#endif  // CORO_EVENTFD_H
