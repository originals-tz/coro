#ifndef COTASK_AWAITER_H
#define COTASK_AWAITER_H

#include "scheduler.h"

class BaseAwaiter
{
public:
    BaseAwaiter() = default;
    virtual ~BaseAwaiter() = default;

    bool await_ready() const { return false; }

    void await_suspend(std::coroutine_handle<TaskPromise> handle)
    {
        auto exec = Executor::ThreadLocalInstance();
        assert(exec);
        m_handle = handle;
        Handle();
    }

    void await_resume() {}

    virtual void Handle() = 0;

    void Resume() { Executor::ResumeCoroutine(m_handle); }

private:
    std::coroutine_handle<TaskPromise> m_handle;
};

#endif  // COTASK_AWAITER_H
