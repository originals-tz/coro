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

## 用法

根据需求把libevent对应的事件封装为awaiter, 然后在协程中使用, 详情看test/example