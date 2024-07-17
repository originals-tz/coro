#include "sleep.h"

namespace coro
{
Sleep::Sleep(int sec, int ms)
    : m_tv({.tv_sec = sec, .tv_usec = ms * 1000})
{}

Sleep::~Sleep()
{
    if (m_event)
    {
        event_free(m_event);
    }
}

void Sleep::Handle()
{
    if (!m_event)
    {
        m_event = evtimer_new(EventBase(), OnTimeout, this);
    }
    evtimer_add(m_event, &m_tv);
}

void Sleep::OnTimeout(int, short, void* arg)
{
    auto pthis = static_cast<Sleep*>(arg);
    pthis->Resume();
}

}