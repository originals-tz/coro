#ifndef COTASK_TASK_H
#define COTASK_TASK_H

#include <coroutine>
#include <evdns.h>
class Task;
struct TaskPromise
{
    Task get_return_object();
    std::suspend_never initial_suspend();
    std::suspend_always final_suspend() noexcept;
    void return_void();
    void unhandled_exception();
    template <typename T>
    T await_transform(T&& t) {
        int i = 0;
        return std::forward<T>(t);
    }
    std::coroutine_handle<> m_parent;
};

class Task
{
public:
    using promise_type = TaskPromise;
    explicit Task(TaskPromise* promise) noexcept;
    ~Task();

    Task(Task&) = delete;
    Task &operator=(Task&) = delete;
    Task(Task &&task) noexcept;
    Task &operator=(Task&& task) noexcept;

    void Resume();
    bool Done() const;

    bool await_ready() const;
    void await_suspend(std::coroutine_handle<> handle);
    void await_resume();
private:
    TaskPromise* m_promise;
    std::coroutine_handle<TaskPromise> m_handle;
};

#endif //COTASK_TASK_H
