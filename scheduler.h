#ifndef COTASK_SCHEDULER_H
#define COTASK_SCHEDULER_H

#include "task.h"
#include <optional>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <thread>
#include <latch>
#include <list>
#include "cassert"
#include <vector>

class CoTask
{
public:
    CoTask() = default;
    virtual ~CoTask() = default;
    virtual Task CoHandle() = 0;
    void SetTask(Task&& task) {m_task = std::move(task);}
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

    ~Context()
    {
        close(m_task_list_fd);
    }


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

class Worker
{
public:
    explicit Worker(std::shared_ptr<Context> ctx)
        : m_ctx(std::move(ctx))
    {
        m_thread = std::make_shared<std::jthread>(&Worker::Run, this);
    }

    ~Worker()
    {
        if (m_thread->joinable())
        {
            m_thread->join();
        }
    }

    void Run()
    {
        m_base = event_base_new();
        EventBase(m_base);
        m_list_ev = event_new(m_base, m_ctx->m_task_list_fd, EV_READ | EV_PERSIST, NewTaskEventCallback, this);
        event_add(m_list_ev, nullptr);

        m_quit_fd = GetFD();
        m_quit_ev = event_new(m_base, m_quit_fd, EV_READ | EV_PERSIST, QuitCallback, this);
        event_add(m_quit_ev, nullptr);

        int val = event_base_dispatch(m_base);
        assert(val != -1);

        event_base_free(m_base);
        event_free(m_list_ev);
        event_free(m_quit_ev);
        close(m_quit_fd);
        m_ctx->m_latch.count_down();
    }

    void Stop()
    {
        eventfd_write(m_quit_fd, eventfd_t(1));
    }

    static event_base* EventBase(event_base* base = nullptr)
    {
        static thread_local event_base* m_base = nullptr;
        if (base != nullptr)
        {
            m_base = base;
        }
        return m_base;
    }
private:

    static void NewTaskEventCallback(evutil_socket_t fd, short event, void* arg)
    {
        auto pthis = static_cast<Worker*>(arg);
        pthis->RunTask();
    }

    static void QuitCallback(evutil_socket_t fd, short event, void* arg)
    {
        auto pthis = static_cast<Worker*>(arg);
        event_base_loopbreak(pthis->m_base);
    }

    void RunTask()
    {
        while(auto task = m_ctx->Get())
        {
            m_task_vect.emplace_back(task);
            task->SetTask(task->CoHandle());
        }
    }

    std::shared_ptr<Context> m_ctx;
    event_base* m_base = nullptr;
    event* m_list_ev = nullptr;
    event* m_quit_ev = nullptr;
    int32_t m_quit_fd = -1;
    std::vector<std::shared_ptr<CoTask>> m_task_vect;
    std::shared_ptr<std::jthread> m_thread;
};

class Scheduler
{
public:
    Scheduler()
    {
        m_ctx = std::make_shared<Context>(1);
        m_worker.emplace_back(m_ctx);
    }

    ~Scheduler()
    {
        for (auto& worker : m_worker)
        {
            worker.Stop();
        }
        m_ctx->m_latch.wait();
    }

    void AddTask(const std::shared_ptr<CoTask>& task)
    {
        m_ctx->Add(task);
    }
private:
    std::shared_ptr<Context> m_ctx;
    std::list<Worker> m_worker;
};

#endif  // COTASK_SCHEDULER_H
