#ifndef COTASK_SCHEDULER_H
#define COTASK_SCHEDULER_H

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <functional>
#include <latch>
#include <list>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>
#include "cassert"
#include "common.h"
#include "iostream"
#include "task.h"

class CoTask
{
public:
    CoTask() = default;
    virtual ~CoTask() = default;
    virtual Task<void> CoHandle() = 0;
    bool Run()
    {
        m_task = CoHandle();
        return m_task->IsDone();
    }

    void SetDeleter(std::function<void()> del) { m_task->m_handle.promise().m_deleter = del; }

    std::optional<Task<void>> m_task;
};

struct Context
{
    explicit Context(long i);
    ~Context();
    void Add(const std::shared_ptr<CoTask>& task);
    std::shared_ptr<CoTask> Get();

    std::mutex m_mut;
    int32_t m_task_list_fd = -1;
    std::latch m_latch;
    std::list<std::shared_ptr<CoTask>> m_task_list;
};

class Executor
{
public:
    explicit Executor(std::shared_ptr<Context> ctx);
    ~Executor();
    void Run();
    void Stop();
    static Executor* ThreadLocalInstance(Executor* ptr = nullptr);
    template <typename T>
    static void ResumeCoroutine(T& handle)
    {
        if (!handle)
            return;

        auto& promise = handle.promise();
        handle.resume();
        if (!handle.done())
        {
            return;
        }

        auto cur = std::move(promise.m_prev);
        if (!cur && promise.m_deleter)
        {
            promise.m_deleter();
            return;
        }
        while (cur)
        {
            // 当前协程已执行完毕, 恢复上一个协程
            cur->Resume();
            if (!cur->Done())
            {
                break;
            }
            // 执行完毕，再切换到上一层
            cur = cur->Prev();
        }
    }
    event_base* GetEventBase();
    void AutoFree(event* ev);

private:
    static void NewTaskEventCallback(evutil_socket_t fd, short event, void* arg);
    static void QuitCallback(evutil_socket_t fd, short event, void* arg);
    void RunTask();

    std::shared_ptr<Context> m_ctx;
    event_base* m_base = nullptr;
    event* m_list_ev = nullptr;
    event* m_quit_ev = nullptr;
    int32_t m_quit_fd = -1;
    std::unordered_map<void*, std::shared_ptr<CoTask>> m_task_vect;
    std::shared_ptr<std::jthread> m_thread;
    std::vector<event*> m_ev_list;
};

class Manager
{
public:
    Manager();
    ~Manager();
    void AddTask(const std::shared_ptr<CoTask>& task);

private:
    std::shared_ptr<Context> m_ctx;
    std::list<Executor> m_executor_list;
};

#endif  // COTASK_SCHEDULER_H
