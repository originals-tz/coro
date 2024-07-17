#include "select.h"

#include <utility>

namespace coro
{
MultiEventfd::MultiEventfd(std::vector<int32_t> fd_vect)
    : m_fd(std::move(fd_vect))
{}

MultiEventfd::~MultiEventfd() noexcept
{
    std::for_each(m_ev.begin(), m_ev.end(), [](event* e){ event_free(e); });
}

void MultiEventfd::Handle()
{
    for (auto& fd : m_fd)
    {
        auto ev = event_new(EventBase(), fd, EV_READ, OnRead, this);
        event_add(ev, nullptr);
        m_ev.emplace_back(ev);
    }
}

void MultiEventfd::OnRead(int, short, void* arg)
{
    auto pthis = static_cast<MultiEventfd*>(arg);
    for (auto& ev : pthis->m_ev)
    {
        event_del(ev);
    }
    pthis->Resume();
}

}