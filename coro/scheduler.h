#ifndef COTASK_SCHEDULER_H
#define COTASK_SCHEDULER_H

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <functional>
#include <latch>
#include <list>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>
#include "cassert"
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
    bool Run()
    {
        m_task = CoHandle();
        return m_task->IsDone();
    }

    /**
     * @brief 设置句柄销毁函数
     * @param del
     */
    void SetDeleter(std::function<void()> del) { m_task->m_handle.promise().m_deleter = del; }

    //! 被挂起的协程
    std::optional<Task<void>> m_task;
};

/**
 * @brief 简单的协程任务，传入一个lambda即可执行，需要注意参数应按值复制
 */
struct SimpleTask : CoTask
{
    SimpleTask(const std::function<Task<void>()>& task)
        : m_user_task(task)
    {}

    Task<void> CoHandle() override
    {
        co_await m_user_task();
        co_return;
    }

    std::function<Task<void>()> m_user_task;
};

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
        if (!task->Run())
        {
            void* ptr = task.get();
            m_task_vect[ptr] = task;
            task->SetDeleter([this, ptr]() { m_task_vect.erase(ptr); });
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
    size_t GetTaskCount() { return m_task_vect.size(); }

private:
    event_base* m_base = nullptr;
    std::unordered_map<void*, std::shared_ptr<CoTask>> m_task_vect;
};

}  // namespace coro
#endif  // COTASK_SCHEDULER_H
