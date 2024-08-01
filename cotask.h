#ifndef CORO_COTASK_H
#define CORO_COTASK_H

#include "task.h"

namespace coro
{
class Executor;
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
    bool Run(Executor* exec);

    //! 被挂起的协程
    std::optional<Task<void>> m_task;
};
}

#endif  // CORO_COTASK_H
