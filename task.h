#ifndef COTASK_TASK_H
#define COTASK_TASK_H

#include <evdns.h>
#include <coroutine>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

template <typename RET>
class Task;
template <typename RET>
class TaskAwaiter;

template <typename T>
struct PromiseRes
{
    void return_value(T ret) { m_ret = ret; }
    std::optional<T> m_ret;
};

template <>
struct PromiseRes<void>
{
    void return_void() {}
};

class HandlerBase
{
public:
    HandlerBase() = default;
    virtual ~HandlerBase() = default;
    virtual void Resume() = 0;
    virtual bool Done() = 0;
    virtual std::unique_ptr<HandlerBase> Prev() = 0;
};

template <typename T>
class Handler : public HandlerBase
{
public:
    Handler(std::coroutine_handle<T> handler)
        : m_handler(handler)
    {}

    void Resume() override { m_handler.resume(); }

    bool Done() override { return m_handler.done(); }

    std::unique_ptr<HandlerBase> Prev() override
    {
        auto& promise = m_handler.promise();
        return std::move(promise.m_prev);
    }

private:
    std::coroutine_handle<T> m_handler;
};

template <typename RET>
struct TaskPromise : public PromiseRes<RET>
{
    Task<RET> get_return_object() { return Task(std::coroutine_handle<TaskPromise>::from_promise(*this)); }

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept
    {
        m_is_final = true;
        return {};
    }

    void unhandled_exception() {}

    template <class TASK_RET>
    TaskAwaiter<TASK_RET> await_transform(Task<TASK_RET>&& task)
    {
        return TaskAwaiter<TASK_RET>(std::move(task));
    }

    template <class T>
    T await_transform(T&& task)
    {
        return task;
    }

    template <class T>
    void Complete(std::coroutine_handle<T> handle)
    {
        if (handle && m_is_final)
        {
            handle.resume();
        }
        else
        {
            m_prev = std::make_unique<Handler<T>>(handle);
        }
    }

    bool Prev()
    {
        if (m_prev)
        {
            m_prev->Resume();
            return true;
        }
        return false;
    }

    std::function<void()> m_deleter;
    bool m_is_final = false;
    std::unique_ptr<HandlerBase> m_prev;
};

template <typename RET>
class Task
{
public:
    using promise_type = TaskPromise<RET>;
    explicit Task(std::coroutine_handle<TaskPromise<RET>> handle) noexcept
        : m_handle(handle)
    {}

    ~Task()
    {
        if (m_handle)
        {
            m_handle.destroy();
        }
    }

    Task(Task&) = delete;
    Task& operator=(Task&) = delete;

    Task(Task&& task) noexcept
        : m_handle(std::exchange(task.m_handle, nullptr))
    {}

    Task& operator=(Task&& task) noexcept
    {
        m_handle = std::exchange(task.m_handle, nullptr);
        return *this;
    }

    bool IsDone() { return m_handle.done(); }
    void Resume() { m_handle.resume(); }

    template <typename T>
    void Finally(std::coroutine_handle<TaskPromise<T>> handle)
    {
        m_handle.promise().Complete(handle);
    }

    operator RET() { return m_handle.promise()->m_ret.get(); }

    std::coroutine_handle<TaskPromise<RET>> m_handle;
};
template <typename RET>
class TaskAwaiter
{
public:
    explicit TaskAwaiter(Task<RET>&& task) noexcept
        : m_task(std::move(task))
    {}
    TaskAwaiter(TaskAwaiter&) = delete;
    TaskAwaiter& operator=(TaskAwaiter&) = delete;
    bool await_ready() const noexcept { return false; }

    template <typename T>
    void await_suspend(std::coroutine_handle<TaskPromise<T>> handle) noexcept
    {
        m_task.Finally(handle);
    }

    RET await_resume() noexcept { return m_task.m_handle.promise().m_ret.value(); }

private:
    Task<RET> m_task;
};

template <>
class TaskAwaiter<void>
{
public:
    explicit TaskAwaiter(Task<void>&& task) noexcept
        : m_task(std::move(task))
    {}
    TaskAwaiter(TaskAwaiter&) = delete;
    TaskAwaiter& operator=(TaskAwaiter&) = delete;
    bool await_ready() const noexcept { return false; }

    template <typename T>
    void await_suspend(std::coroutine_handle<TaskPromise<T>> handle) noexcept
    {
        m_task.Finally(handle);
    }

    void await_resume() noexcept {}

private:
    Task<void> m_task;
};

#endif  // COTASK_TASK_H
