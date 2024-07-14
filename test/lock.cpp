#include <gtest/gtest.h>
#include <mutex.h>
#include "sleep.h"
#include "util.h"

coro::Mutex mut;

std::mutex mut2;

void Log(auto&&... data)
{
    mut2.lock();
    ((std::cout << (data)), ...) << std::endl;
    mut2.unlock();
}

coro::Task<void> DoSomething()
{
    auto id = std::this_thread::get_id();
    for (int i = 0; i < 10; i++)
    {
        Log(id, " in");
        co_await coro::Sleep(1, 200);
        coro::LockGuard lk = co_await mut.Lock();
        Log(id, " out");
    }
}

TEST(coro, mutex)
{
    auto t1 = RunTask(&DoSomething);
    auto t2 = RunTask(&DoSomething);
}