#include "task.h"
#include <fmt/format.h>
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

void TaskPromise::return_void()
{
    if (m_parent)
    {
        m_parent.resume();
    }
}

void TaskPromise::unhandled_exception() {}

TaskAwaiter TaskPromise::await_transform(Task&& task)
{
    return TaskAwaiter(std::move(task));
}

Task::Task(std::coroutine_handle<TaskPromise> handle) noexcept
    : m_handle(handle)
{}

Task::~Task() {}

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
    m_handle.done();
}

void Task::Resume()
{
    m_handle.resume();
}

void Task::Finally(std::coroutine_handle<> handle)
{
    m_handle.promise().m_parent = handle;
}

TaskAwaiter::TaskAwaiter(Task&& task) noexcept
    : task(std::move(task))
{}

TaskAwaiter::TaskAwaiter(TaskAwaiter&& completion) noexcept
    : task(std::exchange(completion.task, Task(std::coroutine_handle<TaskPromise>())))
{}

bool TaskAwaiter::await_ready() const noexcept
{
    return false;
}
void TaskAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
    task.Finally(handle);
}
void TaskAwaiter::await_resume() noexcept {}
