#ifndef CORO_EXECUTOR_H
#define CORO_EXECUTOR_H

#include "cotask.h"

namespace coro
{

class Executor
{
public:
    explicit Executor(event_base* base)
        : m_base(base)
    {}
    /**
     * @brief 调度协程
     * @param handle
     */
    static void ResumeCoroutine(auto&& handle)
    {
        if (!handle)
            return;

        auto& promise = handle.promise();
        handle.resume();
        if (!handle.done())
        {
            return;
        }

        auto cur = std::move(promise.m_prev);
        if (!cur && promise.m_deleter)
        {
            promise.m_deleter();
            return;
        }
        while (cur)
        {
            cur->Resume();
            if (!cur->Done())
            {
                // 未执行完毕，停止调度
                break;
            }

            // 如果执行完毕且为第一个协程，那么释放
            if (cur->IsRoot())
            {
                cur->Free();
                break;
            }

            // 否则获取上一个协程继续执行
            cur = cur->Prev();
        }
    }

    /**
     * @brief 执行cotask
     * @param task
     */
    void RunTask(const std::shared_ptr<CoTask>& task)
    {
        *EventBase() = m_base;
        if (!task->Run())
        {
            void* ptr = task.get();
            m_task_map[ptr] = task;
            task->SetDeleter([this, ptr]() { m_task_map.erase(ptr); });
        }
    }

    /**
     * @brief 执行协程，参数应按值复制
     * @param task
     */
    void RunTask(const std::function<Task<void>()>& task) { RunTask(std::make_shared<SimpleTask>(task)); }

    /**
     * @brief 获取未释放的协程句柄数
     * @return
     */
    size_t GetTaskCount() { return m_task_map.size(); }

    /**
     * @brief 获取当前线程的eventbase
     * @return
     */
    static event_base* LocalEventBase() { return *EventBase(); }

private:
    /**
     * @brief 获取当前线程的eventbase
     * @param base
     * @return
     */
    static event_base** EventBase()
    {
        static thread_local event_base* evbase = nullptr;
        return &evbase;
    }
    //! 事件循环
    event_base* m_base = nullptr;
    //! 挂起的任务列表
    std::unordered_map<void*, std::shared_ptr<CoTask>> m_task_map;
};

}  // namespace coro
#endif  // CORO_EXECUTOR_H
