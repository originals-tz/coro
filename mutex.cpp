#include "mutex.h"
#include <sys/eventfd.h>
#include <utility>
#include "eventfd.h"

namespace coro
{
LockGuard::LockGuard(std::function<void()> unlock)
    : m_unlock(std::move(unlock))
{}

LockGuard::~LockGuard()
{
    if (m_unlock)
    {
        m_unlock();
    }
}

Mutex::Mutex()
    : m_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
{}

Mutex::~Mutex()
{
    close(m_fd);
}

Task<LockGuard&&> Mutex::Lock()
{
    EventFdAwaiter awaiter(m_fd);
    do
    {
        {
            std::lock_guard lk(m_mut);
            if (!m_is_lock)
            {
                m_is_lock = true;
                break;
            }
        }
        co_await awaiter;
    } while (true);
    co_return LockGuard([this] { Unlock(); });
}

void Mutex::Unlock()
{
    eventfd_write(m_fd, 1);
    m_is_lock = false;
}
}  // namespace coro