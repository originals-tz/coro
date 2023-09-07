#include "task.h"
#include <cassert>
#include <utility>

Task TaskPromise::get_return_object()
{
    return Task(this);
}

std::suspend_never TaskPromise::initial_suspend()
{
    return {};
}

std::suspend_never TaskPromise::final_suspend() noexcept
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

Task::Task(TaskPromise* promise) noexcept
    : m_promise(promise)
{}

Task::~Task() {}

Task::Task(Task&& task) noexcept
    : m_promise(std::exchange(task.m_promise, nullptr))
{}

Task& Task::operator=(Task&& task) noexcept
{
    m_promise = task.m_promise;
    return *this;
}

bool Task::await_ready()
{
    std::coroutine_handle<TaskPromise>::from_promise(*m_promise).done();
}

void Task::await_suspend(std::coroutine_handle<> handle)
{
    m_promise->m_parent = handle;
}

void Task::await_resume() {}