#include "task.h"
#include <cassert>
#include <iostream>
#include <utility>

Task TaskPromise::get_return_object()
{
    return Task(std::coroutine_handle<TaskPromise>::from_promise(*this));
}

std::suspend_never TaskPromise::initial_suspend()
{
    return {};
}

std::suspend_always TaskPromise::final_suspend() noexcept
{
    return {};
}

void TaskPromise::return_void() {}

void TaskPromise::unhandled_exception() {}

TaskAwaiter TaskPromise::await_transform(Task&& task)
{
    return TaskAwaiter(std::move(task));
}

Task::Task(std::coroutine_handle<TaskPromise> handle) noexcept
    : m_handle(handle)
{}

Task::~Task()
{
    if (m_handle)
    {
        m_handle.destroy();
    }
}

Task::Task(Task&& task) noexcept
    : m_handle(std::exchange(task.m_handle, nullptr))
{}

Task& Task::operator=(Task&& task) noexcept
{
    m_handle = std::exchange(task.m_handle, nullptr);
    return *this;
}

bool Task::IsDone()
{
    return m_handle.done();
}

void Task::Resume()
{
    m_handle.resume();
}

void Task::Finally(std::coroutine_handle<> handle)
{
    m_handle.promise().Complete(handle);
}

TaskAwaiter::TaskAwaiter(Task&& task) noexcept
    : m_task(std::move(task))
{}

bool TaskAwaiter::await_ready() const noexcept
{
    return false;
}
void TaskAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
    m_task.Finally(handle);
}
void TaskAwaiter::await_resume() noexcept {}
