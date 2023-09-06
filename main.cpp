#include <iostream>
#include "task.h"
#include <event.h>
#include <thread>
#include <utility>
#include <vector>
#include <memory>
#include <list>
#include <sys/eventfd.h>
#include <bits/fcntl.h>
#include <fcntl.h>
#include <optional>

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
    Context()
    {
        m_task_list_fd = GetFD();
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
    std::list<std::shared_ptr<CoTask>> m_task_list;
};

class Worker
{
public:
    explicit Worker(std::shared_ptr<Context> ctx)
        : m_ctx(std::move(ctx))
    {
    }

    void Run()
    {
        m_base = event_base_new();
        m_list_ev = event_new(m_base, m_ctx->m_task_list_fd, EV_READ, NewTaskEventCallback, this);

        m_quit_fd = GetFD();
        m_quit_ev = event_new(m_base, m_quit_fd, EV_READ, QuitCallback, this);

        event_base_dispatch(m_base);
        event_base_free(m_base);
    }

    void Stop()
    {
        eventfd_write(m_quit_fd, eventfd_t(1));
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
};

class Scheduler
{
public:
    Scheduler(size_t worker_size)
    {
    }

    void AddTask(std::shared_ptr<CoTask> task)
    {
    }
private:
    std::vector<std::jthread> m_worker;
};

int main()
{
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
