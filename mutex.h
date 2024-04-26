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
    Mutex() = default;
    ~Mutex()
    {
        while(!m_fd_queue.empty())
        {
            close(m_fd_queue.front());
            m_fd_queue.pop();
        }
    }
    /**
     * @brief 上锁
     * @return 锁的RAII对象
     */
    Task<LockGuard> Lock()
    {
        int32_t fd = -1;
        do
        {
            {
                std::lock_guard lk(m_mut);
                if (!m_is_lock)
                {
                    m_is_lock = true;
                    break;
                }

                if (fd == -1)
                {
                    fd = GetEventFD();
                }
            }
            co_await EventFdAwaiter(fd);
        } while (true);
        ReleaseFD(fd);
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
    /**
     * @brief 获取文件描述符
     * @return 文件描述符
     */
    int GetEventFD()
    {
        std::lock_guard lk(m_mut);
        if (!m_fd_queue.empty())
        {
            int fd = m_fd_queue.front();
            m_fd_queue.pop();
            m_waiting_list.emplace(fd);
            return fd;
        }
        int fd = eventfd(0, 0);
        m_waiting_list.emplace(fd);
        return fd;
    }

    /**
     * @brief 回收文件描述符
     * @param 文件描述符
     */
    void ReleaseFD(int fd)
    {
        if (fd == -1)
        {
            return;
        }
        std::lock_guard lk(m_mut);
        m_waiting_list.erase(fd);
        m_fd_queue.emplace(fd);
    }

    //! 是否上锁
    std::atomic_bool m_is_lock = false;
    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! fd队列
    std::queue<int32_t> m_fd_queue;
    //! 正在等待的fd列表
    std::set<int> m_waiting_list;
};
}  // namespace coro

#endif  // CORO_MUTEX_H
