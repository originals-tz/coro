#ifndef CORO_THREAD_POOL_H
#define CORO_THREAD_POOL_H

#include <event.h>
#include <sys/eventfd.h>
#include <thread>
#include <utility>
#include "eventfd.h"
#include "scheduler.h"

namespace coro
{
struct Context
{
public:
    Context()
    {
        m_fd = Eventfd::Get();
    }

    ~Context()
    {
        close(m_fd);
    }

    /**
     * @brief 获取文件描述符
     * @return
     */
    int GetFD() const
    {
        return m_fd;
    }

    /**
     * @brief 是否停止
     * @return
     */
    bool IsStop()
    {
        return m_stop;
    }

    /**
     * @brief 停止
     */
    void Stop()
    {
        m_stop = true;
        eventfd_write(m_fd, 1);
    }

    /**
     * @bbrief 获取任务
     * @return
     */
    std::shared_ptr<CoTask> Pop()
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

    /**
     * @brief 添加任务
     * @param task
     */
    void Push(const std::shared_ptr<CoTask>& task)
    {
        std::lock_guard lk(m_mut);
        m_task_queue.emplace(task);
        eventfd_write(m_fd, 1);
    }
private:
    //! 用于通知的文件描述符
    int m_fd = -1;
    //! 是否停止
    std::atomic_bool m_stop = false;
    //! 互斥锁
    std::mutex m_mut;
    //! 任务队列
    std::queue<std::shared_ptr<CoTask>> m_task_queue;
};

class Worker
{
public:
    explicit Worker(std::shared_ptr<Context> ctx, int32_t id)
        : m_id(id)
        , m_ctx(std::move(ctx))
        , m_thread(&Worker::Run, this)
    {
    }

private:
    /**
     * @brief 运行工作线程
     */
    void Run()
    {
        m_base = event_base_new();
        m_exec = std::make_unique<Executor>(m_base);
        auto ev = event_new(m_base, m_ctx->GetFD(), EV_READ | EV_PERSIST, OnNotify, this);
        event_add(ev, nullptr);
        event_base_dispatch(m_base);
        event_free(ev);
        event_base_free(m_base);
    }

    /**
     * @brief 收到通知
     * @param ptr
     */
    static void OnNotify(evutil_socket_t, short, void* ptr)
    {
        auto* pthis = static_cast<Worker*>(ptr);
        pthis->Handle();
    }

    /**
     * @brief 处理数据
     */
    void Handle()
    {
        eventfd_t val = 0;
        eventfd_read(m_ctx->GetFD(), &val);
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
    //! 线程id
    int32_t m_id = 0;
    //! 线程上下文
    std::shared_ptr<Context> m_ctx;
    //! 线程本体
    std::jthread m_thread;
    //! 事件循环
    event_base* m_base = nullptr;
    //! 协程执行器
    std::unique_ptr<Executor> m_exec;
};

class ThreadPool
{
public:
    explicit ThreadPool(size_t num)
        : m_num(num)
    {
        for (size_t i = 0; i < m_num; i++)
        {
            auto ctx = std::make_shared<Context>();
            m_thread_pool.emplace_back(std::make_unique<Worker>(ctx, i));
            m_ctx_vect.emplace_back(ctx);
        }
    }

    ~ThreadPool()
    {
        for (auto& ctx : m_ctx_vect)
        {
            ctx->Stop();
        }
    }

    /**
     * @brief 添加任务
     * @param task
     */
    void Add(const std::function<Task<void>()>& task)
    {
        std::lock_guard lk(m_mut);
        m_ctx_vect[m_idx]->Push(std::make_shared<SimpleTask>(task));
        m_idx = (m_idx + 1) % m_num;
    }

    /**
     * @brief 添加任务
     * @param task
     */
    void Add(const std::shared_ptr<CoTask>& task)
    {
        std::lock_guard lk(m_mut);
        m_ctx_vect[m_idx]->Push(task);
        m_idx = (m_idx + 1) % m_num;
    }
private:
    //! 互斥锁
    std::mutex m_mut;
    //! 线程数量
    size_t m_num = 0;
    //! 当前的线程索引
    size_t m_idx = 0;
    //! 上下文
    std::vector<std::shared_ptr<Context>> m_ctx_vect;
    //! 工作线程
    std::vector<std::unique_ptr<Worker>> m_thread_pool;
};
}

#endif  // CORO_THREAD_POOL_H
