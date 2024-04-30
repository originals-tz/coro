#include <channel.h>
#include <gtest/gtest.h>
#include "sleep.h"
#include "util.h"

std::shared_ptr<coro::Channel<int>> chan;

coro::Task<void> CoRead()
{
    int val;
    while (co_await chan->Pop(val) != coro::e_chan_close)
    {
        std::cout << "get " << val << std::endl;
    }
}

coro::Task<void> CoWrite()
{
    for (int i = 0; i < 10; i++)
    {
        chan->Push(i);
        co_await coro::Sleep(0, 200);
    }
}


TEST(coro, chan)
{
    chan = std::make_shared<coro::Channel<int>>();
    auto t1 = RunTask(&CoRead);
    auto t2 = RunTask(&CoWrite);
    sleep(3);
    chan->Close();
}
