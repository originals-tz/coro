#ifndef CORO_EXECUTOR_H
#define CORO_EXECUTOR_H

#include "cotask.h"
#include "event2/event.h"

namespace coro
{

class Executor
{
public:
    explicit Executor(event_base* base)
        : m_base(base)
    {}

    /**
     * @brief 执行cotask
     * @param task
     */
    void RunTask(const std::shared_ptr<CoTask>& task)
    {
        if (!task->Run(m_base))
        {
            void* ptr = task.get();
            m_task_map[ptr] = task;
            task->m_task->SetDestroy([this, ptr]{m_task_map.erase(ptr);});
        }
    }

    /**
     * @brief 执行协程，参数应按值复制
     * @param task
     */
    void RunTask(const std::function<Task<void>()>& task)
    {
        struct T :  CoTask
        {
            T(const std::function<Task<void>()>& t) : m_user_task(t) {}
            Task<void> CoHandle() override
            {
                co_await m_user_task();
                co_return;
            }
            std::function<Task<void>()> m_user_task;
        };
        RunTask(std::make_shared<T>(task));
    }

    /**
     * @brief 获取未释放的协程句柄数
     * @return
     */
    size_t GetTaskCount() { return m_task_map.size(); }
private:

    //! 事件循环
    event_base* m_base = nullptr;
    //! 挂起的任务列表
    std::unordered_map<void*, std::shared_ptr<CoTask>> m_task_map;
};

}  // namespace coro
#endif  // CORO_EXECUTOR_H
