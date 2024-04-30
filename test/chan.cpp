#include <channel.h>
#include <gtest/gtest.h>
#include "sleep.h"
#include "util.h"

std::shared_ptr<coro::Channel<int>> chan;

coro::Task<void> CoRead()
{
    int val;
    while (co_await chan->Pop(val))
    {
        std::cout << "get " << val << std::endl;
    }
    std::cout << "read exit" << std::endl;
    co_return;
}

coro::Task<void> CoWrite()
{
    for (int i = 0; i < 10; i++)
    {
        chan->Push(i);
        co_await coro::Sleep(0, 200);
    }
    co_return;
}

TEST(coro, chan)
{
    chan = std::make_shared<coro::Channel<int>>();
    auto t1 = RunTask(&CoRead);
    auto t2 = RunTask(&CoWrite);
    auto t3 = RunTask(&CoWrite);
    auto t4 = RunTask(&CoRead);
    sleep(3);
    chan->Close();
}

TEST(coro, chan2)
{
    auto ch = std::make_shared<coro::Channel<Data>>();
    ch->Push(Data());
    Data d;
    ch->Push(d);
}

coro::Channel<int> ch2;
coro::Channel<std::string> ch3;

coro::Task<void> SelectRead()
{
    int val;
    std::string str;

    coro::Select select;
    do {
        if (ch2.TryPop(val))
        {
            std::cout << "read" << val << std::endl;
        }

        if (ch3.TryPop(str))
        {
            std::cout << "read str" << str << std::endl;
        }
    } while (co_await select(ch2, ch3));
}

coro::Task<void> WriteVal()
{
    for (int i = 0; i < 10; i++)
    {
        ch2.Push(i);
        co_await coro::Sleep(0, 200);
    }
    co_return;
}

coro::Task<void> WriteStr()
{
    for (int i = 0; i < 10; i++)
    {
        ch3.Push(std::to_string(i));
        co_await coro::Sleep(0, 200);
    }
    co_return;
}


TEST(coro, chan3)
{
    auto t1 = RunTask(&SelectRead);
    auto t2 = RunTask(&WriteVal);
    auto t3 = RunTask(&WriteStr);

    sleep(3);
    ch2.Close();
    ch3.Close();
}
