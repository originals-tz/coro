#ifndef COTASK_SCHEDULER_H
#define COTASK_SCHEDULER_H

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <latch>
#include <list>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include "cassert"
#include "task.h"

class CoTask
{
public:
    CoTask() = default;

    virtual ~CoTask() = default;

    virtual Task CoHandle() = 0;

private:
    std::optional<Task> m_task;
};

int32_t GetFD()
{
    int fd = eventfd(0, 0);
    int opts;
    opts = fcntl(fd, F_GETFL);
    opts = opts | O_NONBLOCK;
    fcntl(fd, F_SETFL, opts);
    return fd;
}

struct Context
{
    explicit Context(long i)
        : m_latch(i)
    {
        m_task_list_fd = GetFD();
    }

    ~Context() { close(m_task_list_fd); }

    void Add(const std::shared_ptr<CoTask>& task)
    {
        std::lock_guard lk(m_mut);
        m_task_list.emplace_back(task);
        eventfd_write(m_task_list_fd, eventfd_t(1));
    }

    std::shared_ptr<CoTask> Get()
    {
        std::lock_guard lk(m_mut);
        if (m_task_list.empty())
        {
            return nullptr;
        }
        auto task = m_task_list.front();
        m_task_list.pop_front();
        return task;
    }

    std::mutex m_mut;
    int32_t m_task_list_fd = -1;
    std::latch m_latch;
    std::list<std::shared_ptr<CoTask>> m_task_list;
};

class Executor
{
public:
    explicit Executor(std::shared_ptr<Context> ctx)
        : m_ctx(std::move(ctx))
    {
        m_thread = std::make_shared<std::jthread>(&Executor::Run, this);
    }

    ~Executor()
    {
        if (m_thread->joinable())
        {
            m_thread->join();
        }
    }

    void Run()
    {
        m_base = event_base_new();
        ThreadLocalInstance(this);
        m_list_ev = event_new(m_base, m_ctx->m_task_list_fd, EV_READ | EV_PERSIST, NewTaskEventCallback, this);
        event_add(m_list_ev, nullptr);

        m_quit_fd = GetFD();
        m_quit_ev = event_new(m_base, m_quit_fd, EV_READ | EV_PERSIST, QuitCallback, this);
        event_add(m_quit_ev, nullptr);

        int val = event_base_dispatch(m_base);
        assert(val != -1);

        event_free(m_list_ev);
        event_free(m_quit_ev);
        for (auto& ev : m_ev_list)
        {
            if (ev)
            {
                event_free(ev);
            }
        }
        event_base_free(m_base);
        close(m_quit_fd);
        m_ctx->m_latch.count_down();
    }

    void Stop() { eventfd_write(m_quit_fd, eventfd_t(1)); }

    static Executor* ThreadLocalInstance(Executor* ptr = nullptr)
    {
        static thread_local Executor* exec = nullptr;
        if (ptr != nullptr)
        {
            exec = ptr;
        }
        return exec;
    }

    event_base* GetEventBase() { return m_base; }

    void AutoFree(event* ev) { m_ev_list.emplace_back(ev); }

private:
    static void NewTaskEventCallback(evutil_socket_t fd, short event, void* arg)
    {
        auto pthis = static_cast<Executor*>(arg);
        pthis->RunTask();
    }

    static void QuitCallback(evutil_socket_t fd, short event, void* arg)
    {
        auto pthis = static_cast<Executor*>(arg);
        event_base_loopbreak(pthis->m_base);
    }

    void RunTask()
    {
        while (auto task = m_ctx->Get())
        {
            m_task_vect.emplace_back(task);
            task->CoHandle();
        }
    }

    std::shared_ptr<Context> m_ctx;
    event_base* m_base = nullptr;
    event* m_list_ev = nullptr;
    event* m_quit_ev = nullptr;
    int32_t m_quit_fd = -1;
    std::vector<std::shared_ptr<CoTask>> m_task_vect;
    std::shared_ptr<std::jthread> m_thread;
    std::vector<event*> m_ev_list;
};

class Manager
{
public:
    Manager()
    {
        m_ctx = std::make_shared<Context>(1);
        m_executor_list.emplace_back(m_ctx);
    }

    ~Manager()
    {
        for (auto& exectuor : m_executor_list)
        {
            exectuor.Stop();
        }
        m_ctx->m_latch.wait();
    }

    void AddTask(const std::shared_ptr<CoTask>& task) { m_ctx->Add(task); }

private:
    std::shared_ptr<Context> m_ctx;
    std::list<Executor> m_executor_list;
};

#endif  // COTASK_SCHEDULER_H
