#ifndef CORO_MUTEX_H
#define CORO_MUTEX_H
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include "awaiter.h"
#include "eventfd.h"
#include "scheduler.h"
#include "task.h"

namespace coro
{
class LockGuard
{
public:
    //! 不允许拷贝，只允许移动
    LockGuard(const LockGuard& x) = delete;
    LockGuard(LockGuard&& x) = default;
    explicit LockGuard(std::function<void()> unlock)
        : m_unlock(std::move(unlock))
    {}
    ~LockGuard()
    {
        if (m_unlock)
        {
            m_unlock();
        }
    }

private:
    //! 解锁函数
    std::function<void()> m_unlock;
};

class Mutex
{
public:
    Mutex()
        : m_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
    {}

    ~Mutex() { close(m_fd); }
    /**
     * @brief 上锁
     * @return 锁的RAII对象
     */
    Task<LockGuard> Lock()
    {
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
            co_await EventFdAwaiter(m_fd);
        } while (true);
        co_return LockGuard([this] { Unlock(); });
    }

    /**
     * @brief 解锁
     */
    void Unlock() { m_is_lock = false; }

private:
    //! eventfd
    int32_t m_fd = 0;
    //! 是否上锁
    std::atomic_bool m_is_lock = false;
    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
};
}  // namespace coro

#endif  // CORO_MUTEX_H
