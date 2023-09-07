#ifndef COTASK_TASK_H
#define COTASK_TASK_H

#include <evdns.h>
#include <coroutine>
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

    std::coroutine_handle<> m_parent;
};

class Task
{
public:
    using promise_type = TaskPromise;

    explicit Task(std::coroutine_handle<TaskPromise> handle) noexcept;

    ~Task();

    Task(Task&) = delete;
    Task& operator=(Task&) = delete;

    Task(Task&& task) noexcept;

    Task& operator=(Task&&) noexcept;

    void Finally(std::coroutine_handle<> handle);

    void await_resume();

    bool IsDone();

    void Resume();

private:
    std::coroutine_handle<TaskPromise> m_handle;
};

class TaskAwaiter
{
public:
    explicit TaskAwaiter(Task&& task) noexcept
        : task(std::move(task))
    {}

    TaskAwaiter(TaskAwaiter&& completion) noexcept
        : task(std::exchange(completion.task, Task(std::coroutine_handle<TaskPromise>())))
    {}

    TaskAwaiter(TaskAwaiter&) = delete;
    TaskAwaiter& operator=(TaskAwaiter&) = delete;
    constexpr bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) noexcept { task.Finally(handle); }
    void await_resume() noexcept {}

private:
    Task task;
};

#endif  // COTASK_TASK_H
