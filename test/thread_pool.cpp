#include "thread_pool.h"
#include <gtest/gtest.h>
#include "util.h"
#include "sleep.h"

coro::Task<void> Sleep()
{
    std::cout << "run in " << std::this_thread::get_id() << std::endl;
    co_await coro::Sleep(1, 11);
    co_return;
}

TEST(t, pool)
{
    coro::ThreadPool pool(4);
    for (auto i = 0; i < 20; i++)
    {
        printf("add task\n");
        pool.Add([]{return Sleep();});
    }
    sleep(3);
}