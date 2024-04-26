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
    /**
     * @brief 上锁
     * @return 锁的RAII对象
     */
    Task<LockGuard> Lock()
    {
        fd_t fd;
        do
        {
            {
                std::lock_guard lk(m_mut);
                if (!m_is_lock)
                {
                    m_is_lock = true;
                    break;
                }

                if (!fd)
                {
                    fd = m_fd_mgr.Acquire();
                    m_waiting_list.emplace(*fd);
                }
            }
            co_await EventFdAwaiter(*fd);
        } while (true);
        m_waiting_list.erase(*fd);
        co_return LockGuard([this] { Unlock(); });
    }

    /**
     * @brief 解锁
     */
    void Unlock()
    {
        std::lock_guard lk(m_mut);
        if (!m_waiting_list.empty())
        {
            int fd = *m_waiting_list.begin();
            eventfd_t val = 1;
            eventfd_write(fd, val);
        }
        m_is_lock = false;
    }

private:
    //! 是否上锁
    std::atomic_bool m_is_lock = false;
    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! 正在等待的fd列表
    std::set<int> m_waiting_list;
    //! 文件描述符管理
    EventFdManager m_fd_mgr;
};
}  // namespace coro

#endif  // CORO_MUTEX_H
