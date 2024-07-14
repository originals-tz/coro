#ifndef CORO_COTASK_H
#define CORO_COTASK_H

#include "task.h"

namespace coro
{
class CoTask
{
public:
    CoTask() = default;
    virtual ~CoTask() = default;
    virtual Task<void> CoHandle() = 0;

    /**
     * @brief 执行协程任务
     * @return
     */
    bool Run(event_base* base)
    {
        m_task = CoHandle();
        m_task->SetEventBase(base);
        m_task->resume();
        return m_task->is_ready();
    }

    //! 被挂起的协程
    std::optional<Task<void>> m_task;
};

/**
 * @brief 简单的协程任务，传入一个lambda即可执行，需要注意参数应按值复制
 */
class SimpleTask : public CoTask
{
public:
    explicit SimpleTask(const std::function<Task<void>()>& task)
        : m_user_task(task)
    {}

    /**
     * @brief 执行协程任务
     * @return
     */
    Task<void> CoHandle() override
    {
        co_await m_user_task();
        co_return;
    }

private:
    //! 用户的任务
    std::function<Task<void>()> m_user_task;
};


}

#endif  // CORO_COTASK_H
