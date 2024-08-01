#ifndef CORO_EXECUTOR_H
#define CORO_EXECUTOR_H

#include "cotask.h"
#include "event2/event.h"

namespace coro
{

class Executor
{
public:
    explicit Executor(event_base* base);

    /**
     * @brief 执行cotask
     * @param task
     */
    void RunTask(const std::shared_ptr<CoTask>& task);

    /**
     * @brief 执行协程，参数应按值复制
     * @param task
     */
    void RunTask(const std::function<Task<void>()>& task);

    /**
     * @brief 获取未释放的协程句柄数
     * @return
     */
    size_t GetTaskCount();

    /**
     * @brief 获取事件基
     * @return
     */
    event_base* EventBase();
private:

    //! 事件循环
    event_base* m_base = nullptr;
    //! 挂起的任务列表
    std::unordered_map<void*, std::shared_ptr<CoTask>> m_task_map;
};

}  // namespace coro
#endif  // CORO_EXECUTOR_H
