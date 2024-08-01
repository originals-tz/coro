#ifndef CORO_SELECT_H
#define CORO_SELECT_H

#include "channel.h"

namespace coro
{

class MultiEventfd : public BaseAwaiter
{
public:
    explicit MultiEventfd(std::vector<int32_t> fd_vect);
    ~MultiEventfd() override;
    void Handle() override;
private:
    static void OnRead(evutil_socket_t, short, void* arg);
    std::vector<int32_t> m_fd;
    std::vector<event*> m_ev;
};

template <typename... CHANNEL>
coro::Task<bool> Select(CHANNEL&&... chan)
{
    if (!(... && chan.IsEmpty()))
    {
        co_return true;
    }
    co_await MultiEventfd({chan.GetEventfd()...});
    co_return !(... && chan.IsClose());
}

}

#endif  // CORO_SELECT_H
