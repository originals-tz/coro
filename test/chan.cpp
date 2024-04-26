#include <channel.h>
#include <gtest/gtest.h>
#include "awaiter.h"
#include "scheduler.h"
#include "sleep.h"
#include "task.h"

std::shared_ptr<coro::Channel<int>> chan;

coro::Task<void> CoRead()
{
    int val;
    while(co_await chan->Pop(val) != coro::e_chan_close)
    {
        std::cout << "get " << val << std::endl;
    }
}

void AddReadTask(evutil_socket_t, short, void* ptr)
{
    auto* exec = static_cast<coro::Executor*>(ptr);
    exec->RunTask([] {return CoRead();});
}

coro::Task<void> CoWrite()
{
    for (int i = 0; i < 10; i++)
    {
        chan->Push(i);
        co_await coro::Sleep(0, 200);
    }
}

void AddWriteTask(evutil_socket_t, short, void* ptr)
{
    auto* exec = static_cast<coro::Executor*>(ptr);
    exec->RunTask([] {return CoWrite();});
}

TEST(coro, chan)
{
    chan = std::make_shared<coro::Channel<int>>();
    auto read_task = [](){
        auto base = event_base_new();
        coro::Executor exec(base);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        event_base_once(base, -1, EV_TIMEOUT, AddReadTask, &exec, &tv);
        event_base_dispatch(base);
        event_base_free(base);
    };

    auto write_task = [](){
        auto base = event_base_new();
        coro::Executor exec(base);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        event_base_once(base, -1, EV_TIMEOUT, AddWriteTask, &exec, &tv);
        event_base_dispatch(base);
        event_base_free(base);
    };
    std::thread t1(read_task);
    std::thread t2(write_task);
    sleep(3);
    chan->Close();
    t1.join();
    t2.join();
}
