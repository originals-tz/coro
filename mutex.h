#ifndef CORO_MUTEX_H
#define CORO_MUTEX_H

#include <atomic>
#include <functional>
#include "task.h"
namespace coro
{
class LockGuard
{
public:
    LockGuard(const LockGuard& x) = delete;
    LockGuard(LockGuard&& x) = default;
    explicit LockGuard(std::function<void()> unlock);
    ~LockGuard();
private:
    //! 解锁函数
    std::function<void()> m_unlock;
};

class Mutex
{
public:
    Mutex();
    ~Mutex();

    /**
     * @brief 锁定互斥体
     * @return 互斥体包装器
     */
    Task<LockGuard&&> Lock();

    /**
     * @brief 解锁互斥体
     */
    void Unlock();
private:
    //! 互斥锁
    std::mutex m_mut;
    //! event fd
    int32_t m_fd = 0;
    //! 是否上锁
    std::atomic_bool m_is_lock = false;
};
}  // namespace coro

#endif  // CORO_MUTEX_H
