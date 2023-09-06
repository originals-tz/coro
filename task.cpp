#include <utility>
#include "task.h"


Task TaskPromise::get_return_object()
{
    return Task(this);
}

std::suspend_never TaskPromise::initial_suspend()
{
    return {};
}

std::suspend_always TaskPromise::final_suspend() noexcept
{
    return {};
}

void TaskPromise::return_void()
{
    if (m_parent)
    {
        m_parent.resume();
    }
}

void TaskPromise::unhandled_exception() {}


Task::Task(TaskPromise *promise) noexcept
    : m_promise(promise)
    , m_handle(std::coroutine_handle<TaskPromise>::from_promise(*promise))
{}

Task::~Task() {}

Task::Task(Task &&task) noexcept
    : m_promise(std::exchange(task.m_promise, nullptr))
    , m_handle(std::exchange(task.m_handle, {}))
{}

Task &Task::operator=(Task &&task) noexcept
{
    m_promise = task.m_promise;
    m_handle = task.m_handle;
    return *this;
}

void Task::Resume()
{
    m_handle.resume();
}

bool Task::Done() const
{
    return m_handle.done();
}

bool Task::await_ready() const
{
    return Done();
}

void Task::await_suspend(std::coroutine_handle<> handle)
{
    m_promise->m_parent = handle;
}

void Task::await_resume() {}