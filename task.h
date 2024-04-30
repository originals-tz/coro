/**
 * @brief 该文件包含了c++20协程核心的几个组件，详情查阅文档https://zh.cppreference.com/w/cpp/language/coroutines
 */
#ifndef CORO_TASK_H
#define CORO_TASK_H

#include <evdns.h>
#include <fmt/format.h>
#include <coroutine>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

namespace coro
{

template <typename RET>
class Task;
template <typename RET>
class TaskAwaiter;

/**
 * @brief 协程返回值
 * @tparam T
 */
template <typename T>
struct PromiseRes
{
    void return_value(T ret) { m_ret.emplace(std::move(ret)); }
    //! 存储返回值
    std::optional<T> m_ret;
};

/**
 * @brief 协程空返回值
 */
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
    /**
     * @brief 恢复协程
     */
    virtual void Resume() = 0;
    /**
     * @brief 协程是否完成
     * @return 是否完成
     */
    virtual bool Done() = 0;
    /**
     * @brief 获取上一个协程
     * @return 协程句柄
     */
    virtual std::unique_ptr<HandlerBase> Prev() = 0;
    /**
     * @brief 是否为根节点
     * @return 是否为根节点
     */
    virtual bool IsRoot() = 0;
    /**
     * @brief 释放协程
     */
    virtual void Free() = 0;
};

template <typename T>
class Handler : public HandlerBase
{
public:
    Handler(std::coroutine_handle<T> handler)
        : m_handler(handler)
    {}

    /**
     * @brief 恢复协程
     */
    void Resume() override { m_handler.resume(); }

    /**
     * @brief 协程是否完成
     * @return 协程是否完成
     */
    bool Done() override { return m_handler.done(); }

    /**
     * @brief 获取上一个协程
     * @return 协程句柄
     */
    std::unique_ptr<HandlerBase> Prev() override
    {
        auto& promise = m_handler.promise();
        return std::move(promise.m_prev);
    }

    /**
     * @brief 是否为根节点
     * @return 是否为根节点
     */
    bool IsRoot() override { return m_handler.promise().m_prev == nullptr; }

    /**
     * @brief 释放协程
     */
    void Free() override
    {
        auto& deleter = m_handler.promise().m_deleter;
        if (deleter)
        {
            deleter();
        }
    }

private:
    //! 协程句柄
    std::coroutine_handle<T> m_handler;
};

template <typename RET>
struct TaskPromise : public PromiseRes<RET>
{
#ifdef CO_DEBUG
    TaskPromise()
    {
        auto ptr = (void*)this;
        std::cout << fmt::format("Promise : {}", ptr) << std::endl;
    }

    ~TaskPromise()
    {
        auto ptr = (void*)this;
        std::cout << fmt::format("Release Promise : {}", ptr) << std::endl;
    }
#endif

    /**
     * @brief 获取promise对象
     * @return
     */
    Task<RET> get_return_object() { return Task(std::coroutine_handle<TaskPromise>::from_promise(*this)); }

    /**
     * @brief 初始化
     * @return
     */
    std::suspend_never initial_suspend() { return {}; }

    /**
     * @brief 协程结束
     * @return
     */
    std::suspend_always final_suspend() noexcept
    {
        m_is_final = true;
        return {};
    }

    /**
     * @brief 异常处理
     */
    void unhandled_exception() {}

    /**
     * @brief 转换为可等待体
     * @tparam TASK_RET
     * @param task
     * @return
     */
    template <class TASK_RET>
    TaskAwaiter<TASK_RET> await_transform(Task<TASK_RET>&& task)
    {
        return TaskAwaiter<TASK_RET>(std::move(task));
    }

    /**
     * @brief 转换为可等待体
     * @tparam TASK_RET
     * @param task
     * @return
     */
    template <class T>
    T await_transform(T&& task)
    {
        return task;
    }

    /**
     * @brief 协程完成
     * @tparam T
     * @param handle
     */
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

    /**
     * @brief 恢复上一个协程
     * @return
     */
    bool Prev()
    {
        if (m_prev)
        {
            m_prev->Resume();
            return true;
        }
        return false;
    }

    //! 资源释放
    std::function<void()> m_deleter;
    //! 是否完成
    bool m_is_final = false;
    //! 上一个协程
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

    /**
     * @brief 协程上是否完成
     * @return
     */
    bool IsDone() { return m_handle.done(); }

    /**
     * @brief 恢复协程
     */
    void Resume() { m_handle.resume(); }

    /**
     * @brief 协程结束
     * @tparam T
     * @param handle
     */
    template <typename T>
    void Finally(std::coroutine_handle<TaskPromise<T>> handle)
    {
        m_handle.promise().Complete(handle);
    }

    /**
     * @brief 返回值
     * @return
     */
    operator RET() { return m_handle.promise()->m_ret.get(); }

    //! 协程句柄
    std::coroutine_handle<TaskPromise<RET>> m_handle;
};

/**
 * @brief 可等待体，由返回值
 * @tparam RET
 */
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

    RET&& await_resume() noexcept { return std::move(m_task.m_handle.promise().m_ret.value()); }

private:
    Task<RET> m_task;
};

/**
 * @brief 无返回值的可等待体
 */
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

}  // namespace coro

#endif  // CORO_TASK_H
