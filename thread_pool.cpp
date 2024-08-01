#include "thread_pool.h"

namespace coro
{
ThreadContext::ThreadContext()
    : m_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
{}

ThreadContext::~ThreadContext()
{
    close(m_fd);
}

int ThreadContext::GetEventfd() const
{
    return m_fd;
}

bool ThreadContext::IsStop()
{
    return m_stop;
}

void ThreadContext::Stop()
{
    m_stop = true;
    eventfd_write(m_fd, 1);
}

std::shared_ptr<CoTask> ThreadContext::Pop()
{
    std::lock_guard lk(m_mut);
    if (m_task_queue.empty())
    {
        return nullptr;
    }
    auto task = m_task_queue.front();
    m_task_queue.pop();
    return task;
}

void ThreadContext::Push(const std::shared_ptr<CoTask>& task)
{
    std::lock_guard lk(m_mut);
    m_task_queue.emplace(task);
    eventfd_write(m_fd, 1);
}

Worker::Worker(std::shared_ptr<ThreadContext> ctx, int32_t id)
    : m_id(id)
    , m_ctx(std::move(ctx))
    , m_thread(&Worker::Run, this)
{}

void Worker::Run()
{
    m_base = event_base_new();
    m_exec = std::make_unique<Executor>(m_base);
    auto ev = event_new(m_base, m_ctx->GetEventfd(), EV_READ | EV_PERSIST, OnNotify, this);
    event_add(ev, nullptr);
    event_base_dispatch(m_base);
    event_free(ev);
    event_base_free(m_base);
}

void Worker::OnNotify(evutil_socket_t, short, void* ptr)
{
    auto* pthis = static_cast<Worker*>(ptr);
    pthis->Handle();
}

void Worker::Handle()
{
    eventfd_t val = 0;
    eventfd_read(m_ctx->GetEventfd(), &val);
    if (!m_ctx->IsStop())
    {
        while (auto task = m_ctx->Pop())
        {
            m_exec->RunTask(task);
        }
    }

    if (m_ctx->IsStop())
    {
        event_base_loopbreak(m_base);
        return;
    }
}

ThreadPool::ThreadPool(size_t num)
    : m_num(num)
{
    for (size_t i = 0; i < m_num; i++)
    {
        auto ctx = std::make_shared<ThreadContext>();
        m_thread_pool.emplace_back(std::make_unique<Worker>(ctx, i));
        m_ctx_vect.emplace_back(ctx);
    }
}

ThreadPool::~ThreadPool()
{
    for (auto& ctx : m_ctx_vect)
    {
        ctx->Stop();
    }
}

void ThreadPool::Add(const std::function<Task<void>()>& task)
{
    struct T : CoTask
    {
        explicit T(const std::function<Task<void>()>& t)
            : m_user_task(t)
        {}
        Task<void> CoHandle() override
        {
            co_await m_user_task();
            co_return;
        }
        std::function<Task<void>()> m_user_task;
    };
    std::lock_guard lk(m_mut);
    m_ctx_vect[m_idx]->Push(std::make_shared<T>(task));
    m_idx = (m_idx + 1) % m_num;
}

void ThreadPool::Add(const std::shared_ptr<CoTask>& task)
{
    std::lock_guard lk(m_mut);
    m_ctx_vect[m_idx]->Push(task);
    m_idx = (m_idx + 1) % m_num;
}

}  // namespace coro