#ifndef COTASK_TASK_H
#define COTASK_TASK_H

#include <evdns.h>
#include <coroutine>
#include <iostream>
#include <utility>

class Task;
class TaskAwaiter;

struct TaskPromise
{
    Task get_return_object();
    std::suspend_never initial_suspend();
    std::suspend_always final_suspend() noexcept;
    void return_void();
    void unhandled_exception();
    TaskAwaiter await_transform(Task&& task);
    template <class T>
    T await_transform(T&& task)
    {
        return task;
    }
    void Complete(std::coroutine_handle<> handle)
    {
        if (handle && m_is_final)
        {
            handle.resume();
        }
    }
    bool m_is_final = false;
};

class Task
{
public:
    using promise_type = TaskPromise;
    Task(std::coroutine_handle<TaskPromise> handle) noexcept;
    ~Task();

    Task(Task&) = delete;
    Task& operator=(Task&) = delete;

    Task(Task&& task) noexcept;
    Task& operator=(Task&&) noexcept;

    bool IsDone();
    void Resume();
    void Finally(std::coroutine_handle<> handle);

    std::coroutine_handle<TaskPromise> m_handle;
};

class TaskAwaiter
{
public:
    explicit TaskAwaiter(Task&& task) noexcept;
    TaskAwaiter(TaskAwaiter&) = delete;
    TaskAwaiter& operator=(TaskAwaiter&) = delete;
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> handle) noexcept;
    void await_resume() noexcept;

private:
    Task m_task;
};

#endif  // COTASK_TASK_H
