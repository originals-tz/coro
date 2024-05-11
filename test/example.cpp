#include <gtest/gtest.h>
#include "awaiter.h"
#include "executor.h"
#include "task.h"

event_base* base = nullptr;
std::shared_ptr<coro::Executor> exec;

// 封装libevent事件
struct Sleep : coro::BaseAwaiter<void>
{
    coro::Task<int> Wait(int sec)
    {
        timeval tv{.tv_sec = sec, .tv_usec = 1};
        // 注册libevent事件
        auto ev = evtimer_new(base, OnTimeout, this);
        evtimer_add(ev, &tv);
        co_await *this;  // 切出去
        // 从这里恢复, 继续执行
        event_free(ev);
        co_return sec;
    }

    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        // 在回调函数中恢复协程
        static_cast<Sleep*>(arg)->Resume();
    }
};

// 定义一个协程
coro::Task<void> TestSleep()
{
    std::cout << "sleep" << std::endl;
    Sleep sleep;
    // 调用封装的libevent事件
    co_await sleep.Wait(1);
    std::cout << "sleep end" << std::endl;
    co_return;
}

void AddCoTask(evutil_socket_t, short, void* ptr)
{
    // 开始执行协程
    exec->RunTask([] { return TestSleep(); });
    std::cout << "add task" << std::endl;
}

TEST(example, main)
{
    base = event_base_new();
    exec = std::make_shared<coro::Executor>(base);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    // libevent启动后执行AddCoTask
    event_base_once(base, -1, EV_TIMEOUT, AddCoTask, nullptr, &tv);

    event_base_dispatch(base);
    event_base_free(base);
}