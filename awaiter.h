#ifndef CORO_AWAITER_H
#define CORO_AWAITER_H

#include <functional>
#include <event2/event.h>
#include <cassert>
#include <optional>

namespace coro
{
template <typename RET>
class BaseAwaiter
{
public:
    BaseAwaiter() = default;
    virtual ~BaseAwaiter() = default;

    /**
     * @brief co_await执行后，首先调用这个函数，返回false, 则暂停协程，执行await_suspend
     * @return 返回false，挂起协程
     */
    bool await_ready() const { return false; }

    /**
     * @brief 在此进行业务处理
     * @tparam T 类型
     * @param handle 协程句柄
     */
    template <typename T>
    void await_suspend(T handle)
    {
        // 将句柄绑定到恢复函数中
        m_resume = [handle]() { handle.resume(); };
        Handle();
    }

    /**
     * @brief 业务处理
     */
    virtual void Handle() {}

    /**
     * @brief 恢复协程
     */
    void Resume()
    {
        if (m_resume)
        {
            m_resume();
        }
    }

private:
    //! 恢复函数
    std::function<void()> m_resume;
};

/**
 * @brief 对void进行特化处理
 */
template <>
class BaseAwaiter<void>
{
public:
    BaseAwaiter() = default;
    virtual ~BaseAwaiter() = default;

    /**
     * @brief co_await执行后，首先调用这个函数，返回false, 则暂停协程，执行await_suspend
     * @return 返回false，挂起协程
     */
    bool await_ready() const { return false; }

    /**
     * @brief 在此进行业务处理
     * @tparam T 类型
     * @param handle 协程句柄
     */
    template <typename T>
    void await_suspend(T handle)
    {
        m_resume = [handle]() { handle.resume(); };
        m_base = handle.promise().GetState().lock()->m_base;
        Handle();
    }

    /**
     * @brief 调用结束后，返回空
     */
    void await_resume() {}

    /**
     * @brief 业务处理
     */
    virtual void Handle() {}

    /**
     * @brief 恢复协程
     */
    void Resume()
    {
        if (m_resume)
        {
            m_resume();
        }
    }

    /**
     * @brief 获取eventbase
     */
    event_base* EventBase()
    {
        return m_base;
    }

private:
    //! 事件循环
    event_base* m_base = nullptr;
    //! 恢复函数
    std::function<void()> m_resume;
};

}  // namespace coro

#endif  // CORO_AWAITER_H
