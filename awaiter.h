#ifndef COTASK_AWAITER_H
#define COTASK_AWAITER_H

#include "scheduler.h"

class BaseAwaiter
{
public:
    BaseAwaiter() = default;
    virtual ~BaseAwaiter() = default;

    bool await_ready() const { return false; }

    template <typename T>
    void await_suspend(T handle)
    {
        auto exec = Executor::ThreadLocalInstance();
        assert(exec);
        m_resume = [handle]() {Executor::ResumeCoroutine(handle);};
        Handle();
    }

    int await_resume() {
        return 1;
    }

    virtual void Handle() = 0;

    void Resume() {
        if (m_resume)
        {
            m_resume();
        }
    }
private:
    std::function<void()> m_resume;
};

#endif  // COTASK_AWAITER_H
