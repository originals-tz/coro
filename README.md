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

## Overview

首先从一个例子开始

```cpp
// A任务发起
void GetUserList()
{
    // 获取redis上的用户列表
    m_redis.Execute(&Service::OnGetUserList,"HGETALL %s_%d", RedisKey::m_user_list, datacenter::DataCenter::SrvID());
}

// A任务回调
void OnGetUserList(RedisData data)
{
    UpdateUID update_uid(data);
    if (!update_uid.m_new.empty())
    {
        // B任务发起，获取更新的用户条件 
        m_redis.Execute(&Service::OnGetUserCondition,...);
    }
}

//B任务回调
void OnGetUserCondition()
{
    ...
}
```

在传统的异步编程中，一个任务的发起和处理，由两个函数构成:发起函数，回调函数，然后，如果处理完后还需要发起后续的任务，则会形成一个链条 `A->A'->B->B'`

随着业务的复杂度增加，链条长度也会增加，加上上下文的割裂，用户必须要声明成员变量，供发起函数，回调函数使用，这样一来，心智负担会越来越重

而协程解决了异步编程上下文不连续的问题，让整个业务处理的过程更加连贯, 代码如下

```cpp
coro::Task<void> RedisHandler::CoGetUserList()
{
    CoRedis exec(m_con);
    UpdateUID update_uid;
    // 发起一个协程,从redis上获取所有的用户，检查他们的crc，然后获取新用户，和crc不一致的用户
    // 可以看到，由于上下文是连续的，此处可以直接使用update_uid的引用，而不需要将其作为全局变量，供异步回调函数访问
    auto ret = co_await exec.Execute([&](auto&& x){ return OnGetUserList(x, update_uid);},
                                     "HGETALL %s_%d", RedisKey::m_user_list, datacenter::DataCenter::SrvID());
    // 执行完后直接判断返回值
    if (!ret)
    {
        Trace(eum_LOG_DEBUG, "获取用户列表失败");
        co_return;
    }
    if (!update_uid.m_new.empty())
    {
        // 如果由新用户, 发起协程来从mysql中加载相应的数据
        m_co_exec.RunTask([update_uid]{return Load::CoLoadUserWarnRecord(update_uid.m_new);});
        GetUserCondition(update_uid.m_new);
    }
    co_return;
}
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