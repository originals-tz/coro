#ifndef CORO_THREAD_POOL_H
#define CORO_THREAD_POOL_H

#include <event.h>
#include <sys/eventfd.h>
#include <thread>
#include <utility>
#include "eventfd.h"
#include "executor.h"

namespace coro
{
struct ThreadContext
{
public:
    ThreadContext();
    ~ThreadContext();

    /**
     * @brief 获取文件描述符
     * @return
     */
    int GetEventfd() const;

    /**
     * @brief 是否停止
     * @return
     */
    bool IsStop();

    /**
     * @brief 停止
     */
    void Stop();

    /**
     * @bbrief 获取任务
     * @return
     */
    std::shared_ptr<CoTask> Pop();

    /**
     * @brief 添加任务
     * @param task
     */
    void Push(const std::shared_ptr<CoTask>& task);

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
    explicit Worker(std::shared_ptr<ThreadContext> ctx, int32_t id);

private:
    /**
     * @brief 运行工作线程
     */
    void Run();

    /**
     * @brief 收到通知
     * @param ptr
     */
    static void OnNotify(evutil_socket_t, short, void* ptr);

    /**
     * @brief 处理数据
     */
    void Handle();

    //! 线程id
    int32_t m_id = 0;
    //! 线程上下文
    std::shared_ptr<ThreadContext> m_ctx;
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
    explicit ThreadPool(size_t num);
    ~ThreadPool();

    /**
     * @brief 添加任务
     * @param task
     */
    void Add(const std::function<Task<void>()>& task);

    /**
     * @brief 添加任务
     * @param task
     */
    void Add(const std::shared_ptr<CoTask>& task);

private:
    //! 互斥锁
    std::mutex m_mut;
    //! 线程数量
    size_t m_num = 0;
    //! 当前的线程索引
    size_t m_idx = 0;
    //! 上下文
    std::vector<std::shared_ptr<ThreadContext>> m_ctx_vect;
    //! 工作线程
    std::vector<std::unique_ptr<Worker>> m_thread_pool;
};
}  // namespace coro

#endif  // CORO_THREAD_POOL_H
