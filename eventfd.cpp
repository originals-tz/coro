#include "eventfd.h"
#include <sys/eventfd.h>

namespace coro
{
EventFdAwaiter::EventFdAwaiter(int fd)
    : m_event_fd(fd)
{}

EventFdAwaiter::~EventFdAwaiter()
{
    if (m_event)
    {
        event_free(m_event);
    }
}

void EventFdAwaiter::Handle()
{
    if (!m_event)
    {
        m_event = event_new(EventBase(), m_event_fd, EV_READ, OnRead, this);
    }
    event_add(m_event, nullptr);
}

void EventFdAwaiter::OnRead(evutil_socket_t, short, void* arg)
{
    auto pthis = static_cast<EventFdAwaiter*>(arg);
    eventfd_t val = 0;
    eventfd_read(pthis->m_event_fd, &val);
    pthis->Resume();
}
}