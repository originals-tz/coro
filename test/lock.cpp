#include <gtest/gtest.h>
#include <mutex.h>
#include "sleep.h"
#include "util.h"

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

TEST(coro, mutex)
{
    auto t1 = RunTask(&DoSomething);
    auto t2 = RunTask(&DoSomething);
}