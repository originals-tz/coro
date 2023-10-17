#include "scheduler.h"

Context::Context(long i)
    : m_latch(i)
{
    m_task_list_fd = Common::GetFD();
}

Context::~Context()
{
    close(m_task_list_fd);
}

void Context::Add(const std::shared_ptr<CoTask>& task)
{
    std::lock_guard lk(m_mut);
    m_task_list.emplace_back(task);
    eventfd_write(m_task_list_fd, eventfd_t(1));
}

std::shared_ptr<CoTask> Context::Get()
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

Executor::Executor(std::shared_ptr<Context> ctx)
    : m_ctx(std::move(ctx))
{
    m_thread = std::make_shared<std::jthread>(&Executor::Run, this);
}

Executor::~Executor()
{
    if (m_thread->joinable())
    {
        m_thread->join();
    }
}

void Executor::Run()
{
    m_base = event_base_new();
    ThreadLocalInstance(this);
    m_list_ev = event_new(m_base, m_ctx->m_task_list_fd, EV_READ | EV_PERSIST, NewTaskEventCallback, this);
    event_add(m_list_ev, nullptr);

    m_quit_fd = Common::GetFD();
    m_quit_ev = event_new(m_base, m_quit_fd, EV_READ | EV_PERSIST, QuitCallback, this);
    event_add(m_quit_ev, nullptr);

    int val = event_base_dispatch(m_base);
    std::cout << "未完成任务数:" << m_task_vect.size() << std::endl;
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

void Executor::Stop()
{
    eventfd_write(m_quit_fd, eventfd_t(1));
}

Executor* Executor::ThreadLocalInstance(Executor* ptr)
{
    static thread_local Executor* exec = nullptr;
    if (ptr != nullptr)
    {
        exec = ptr;
    }
    return exec;
}

event_base* Executor::GetEventBase()
{
    return m_base;
}

void Executor::AutoFree(event* ev)
{
    m_ev_list.emplace_back(ev);
}

void Executor::NewTaskEventCallback(evutil_socket_t fd, short event, void* arg)
{
    auto pthis = static_cast<Executor*>(arg);
    pthis->RunTask();
}

void Executor::QuitCallback(evutil_socket_t fd, short event, void* arg)
{
    auto pthis = static_cast<Executor*>(arg);
    event_base_loopbreak(pthis->m_base);
}

void Executor::RunTask()
{
    while (auto task = m_ctx->Get())
    {
        if (!task->Run())
        {
            void* ptr = task.get();
            m_task_vect[ptr] = task;
            task->SetDeleter([this, ptr]() { m_task_vect.erase(ptr); });
        }
        else
        {
            std::cout << "任务已完成" << std::endl;
        }
    }
}

Manager::Manager()
{
    m_ctx = std::make_shared<Context>(1);
    m_executor_list.emplace_back(m_ctx);
}

Manager::~Manager()
{
    for (auto& exectuor : m_executor_list)
    {
        exectuor.Stop();
    }
    m_ctx->m_latch.wait();
}

void Manager::AddTask(const std::shared_ptr<CoTask>& task)
{
    m_ctx->Add(task);
}
