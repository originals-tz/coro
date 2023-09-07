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

void Task::Finally(std::coroutine_handle<> handle)
{
    m_handle.promise().m_parent = handle;
}

void Task::await_resume() {}

bool Task::IsDone()
{
    m_handle.done();
}

void Task::Resume()
{
    m_handle.resume();
}
