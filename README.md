[[_TOC_]]

# Coro

c++ 20 coroutine + libevent 封装

官方文档 : https://zh.cppreference.com/w/cpp/language/coroutines

封装思路

由于是无栈协程，因此一个协程函数结束后，无法自动将控制权转移到调用方，因此, 将所有协程组织成一个链表

每一个协程记录其调用方，在协程结束后，在外部主动唤醒调用方

```
c0 <- c1 <- c2 <- c3

c0 为第一个协程，在外部持有句柄, 需要手动释放
```
## Task & CoTask

`coro::Task<T>` 是协程的封装, 当一个函数的返回值为`coro::Task<T>`, 将其视为一个协程

一个协程会被`coro::CoTask`包裹，传递到Executor中执行

```cpp
#include "coro/scheduler.h"
#include "coro/sleep.h"
std::shared_ptr<coro::Executor> exec;

coro::Task<void> Test()
{
    co_await coro::Sleep(1);
    co_return;
}

struct Task2 : coro::CoTask
{
    coro::Task<void> CoHandle() override
    {
        co_await coro::Sleep(1);
        co_return;
    }
};

void AddTask(evutil_socket_t, short, void* ptr)
{
    // 添加任务的两种方式
    exec->RunTask([]{return Test();}); // 添加一个Lambda
    exec->RunTask(std::make_shared<Task2>()); //添加一个cotask
}

int main()
{
    auto base = event_base_new();
    exec = std::make_shared<coro::Executor>(base);
    event_base_once(base, -1, EV_TIMEOUT, AddTask, nullptr, nullptr);
    event_base_dispatch(base);
    event_base_free(base);
    return 0;
}
```

## Awaiter

Awaiter是`co_await`的操作对象 ,一般会将一些需要等待的操作进行封装

例如`sleep`, 可以将`sleep`封装成一个`awaiter`, 使用libevent中的超时任务实现一个异步的sleep


```cpp
#include "awaiter.h"
#include "scheduler.h"
class Sleep : public coro::BaseAwaiter<void>
{
public:
    Sleep(int sec, int ms = 0)
        : m_sec(sec)
        , m_usec(ms * 1000)
    {}

    void Handle() override
    {
        timeval tv{.tv_sec = m_sec, .tv_usec = m_usec};
        m_event = evtimer_new(Executor::LocalEventBase(), OnTimeout, this);
        evtimer_add(m_event, &tv);
    }

private:
    static void OnTimeout(evutil_socket_t, short, void* arg)
    {
        auto pthis = static_cast<Sleep*>(arg);
        evtimer_del(pthis->m_event);
        event_free(pthis->m_event);
        pthis->Resume();
    }

    int m_sec = 0;
    int m_usec = 0;
    event* m_event = nullptr;
};
```
在程序中进行异步的等待，这种操作可以用于轮询接口的等待，例如mysql的异步接口，在轮询过程中进行sleep，让出cpu处理其他事件

```cpp
    s = mysql_real_connect_nonblocking(...);
    if (s == NET_ASYNC_COMPLETE)
    {
        break;
    }
    else if (s == NET_ASYNC_NOT_READY) 
    {
        // 异步接口未就绪，进行等待
        co_await coro::Sleep(0, 16);
    }
    ...
```

## 其他组件
- `coro::Sleep` : sleep的异步版本
- `coro::Channel<T>` : 在协程与协程，协程与线程间交换数据
- `coro::Mutex` : 互斥锁, 在协程中使用
- `coro::ThreadPool` : 多线程的协程池，可将任务投入池中，由其他线程运行