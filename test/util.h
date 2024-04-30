#ifndef CORO_UTIL_H
#define CORO_UTIL_H

#include <thread>
#include "task.h"
#include "scheduler.h"

struct TaskCtx
{
    void Run()
    {
        m_exec -> RunTask([this]{return (*m_func)();});
    }
    coro::Executor* m_exec = nullptr;
    coro::Task<void>(* m_func)(){};
};

void AddTask(evutil_socket_t, short, void* ptr)
{
    auto* ctx = static_cast<TaskCtx*>(ptr);
    ctx->Run();
}

std::jthread RunTask(coro::Task<void>(* func)())
{
    std::jthread t1([func]{
        auto base = event_base_new();
        coro::Executor exec(base);
        TaskCtx ctx;
        ctx.m_exec = &exec;
        ctx.m_func = func;
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        event_base_once(base, -1, EV_TIMEOUT, AddTask, &ctx, &tv);
        event_base_dispatch(base);
        event_base_free(base);
    });
    return t1;
}

#endif  // CORO_UTIL_H
