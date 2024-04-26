#include <mutex.h>
#include <gtest/gtest.h>
#include "awaiter.h"
#include "scheduler.h"
#include "sleep.h"
#include "task.h"

coro::Mutex mut;

coro::Task<void> DoSomething()
{
    auto id = std::this_thread::get_id();
    for (int i = 0; i < 10; i++)
    {
        co_await coro::Sleep(0, 200);
        coro::LockGuard lk = co_await mut.Lock();
        std::cout << id << "do " << std::endl;
    }
}

void AddTask(evutil_socket_t, short, void* ptr)
{
    auto* exec = static_cast<coro::Executor*>(ptr);
    exec->RunTask([] {return DoSomething();});
}

TEST(coro, mutex)
{
    auto do_something = [](){
        auto base = event_base_new();
        coro::Executor exec(base);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        event_base_once(base, -1, EV_TIMEOUT, AddTask, &exec, &tv);
        event_base_dispatch(base);
        event_base_free(base);
    };
    std::thread t1(do_something);
    std::thread t2(do_something);
    t1.join();
    t2.join();
}
