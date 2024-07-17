#include "executor.h"

namespace coro
{

Executor::Executor(event_base* base)
    : m_base(base)
{}

void Executor::RunTask(const std::shared_ptr<CoTask>& task)
{
    if (!task->Run(m_base))
    {
        void* ptr = task.get();
        m_task_map[ptr] = task;
        task->m_task->SetDestroy([this, ptr]{m_task_map.erase(ptr);});
    }
}

void Executor::RunTask(const std::function<Task<void>()>& task)
{
    struct T :  CoTask
    {
        explicit T(const std::function<Task<void>()>& t) : m_user_task(t) {}
        Task<void> CoHandle() override
        {
            co_await m_user_task();
            co_return;
        }
        std::function<Task<void>()> m_user_task;
    };
    RunTask(std::make_shared<T>(task));
}

size_t Executor::GetTaskCount() { return m_task_map.size(); }
}