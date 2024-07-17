#ifndef CORO_CHANNEL_H
#define CORO_CHANNEL_H

#include <sys/eventfd.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "eventfd.h"
#include "executor.h"
#include "task.h"

namespace coro
{

template <typename T>
class Channel
{
public:
    Channel(const Channel&) = delete;
    Channel();
    ~Channel();

    /**
     * @brief 关闭channel，唤醒协程
     */
    void Close();

    /**
     * @brief 添加一个数据
     * @param t 数据
     * @return 添加成功后返回true
     */
    bool Push(auto&& t);

    /**
     * @brief 获取一个数据
     * @param t 数据引用
     * @return 获取成功后返回true
     */
    Task<bool> Pop(T& t);

    /**
     * @brief 尝试获取数据
     * @param t 数据引用
     * @return 获取成功后返回true
     */
    bool TryPop(T& t);

    /**
     * @brief 判断channel是否关闭
     * @return channel关闭返回true
     */
    bool IsClose();

    /**
     * @brief 判断数据队列是否为空
     * @return 数据队列为空返回true
     */
    bool IsEmpty();

    /**
     * @brief 获取event fd
     * @return event fd
     */
    int32_t GetEventfd();
private:
    //! 通知channel的fd
    int m_fd = -1;
    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! 数据队列
    std::queue<std::optional<T>> m_data_queue;
    //! 数据量
    std::atomic_size_t m_data_count = 0;
    //! 是否关闭
    std::atomic_bool m_is_close = false;
};

template <typename T>
Channel<T>::Channel()
    : m_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
{}

template <typename T>
Channel<T>::~Channel()
{
    close(m_fd);
}

template <typename T>
void Channel<T>::Close()
{
    if (m_is_close)
    {
        return;
    }
    std::lock_guard lk(m_mut);
    m_is_close = true;
    eventfd_write(m_fd, 1);
}

template <typename T>
bool Channel<T>::Push(auto&& t)
{
    if (m_is_close)
    {
        return false;
    }
    std::lock_guard lk(m_mut);
    m_data_queue.emplace(std::forward<decltype(t)>(t));
    m_data_count++;
    eventfd_write(m_fd, 1);
    return true;
}

template <typename T>
Task<bool> Channel<T>::Pop(T& t)
{
    if (m_is_close)
    {
        co_return false;
    }

    bool s = false;
    EventFdAwaiter awaiter(m_fd);
    do
    {
        {
            std::lock_guard lk(m_mut);
            if (!m_data_queue.empty())
            {
                t = std::move(m_data_queue.front().value());
                m_data_queue.pop();
                m_data_count--;
                s = true;
                break;
            }
        }
        co_await awaiter;
    } while (!m_is_close);
    eventfd_write(m_fd, 1);
    co_return s;
}

template <typename T>
bool Channel<T>::TryPop(T& t)
{
    if (m_is_close)
    {
        return false;
    }

    std::lock_guard lk(m_mut);
    if (!m_data_queue.empty())
    {
        t = std::move(m_data_queue.front().value());
        m_data_queue.pop();
        m_data_count--;
        return true;
    }
    return false;
}

template <typename T>
bool Channel<T>::IsClose()
{
    return m_is_close;
}

template <typename T>
bool Channel<T>::IsEmpty()
{
    return m_data_count == 0;
}

template <typename T>
int32_t Channel<T>::GetEventfd()
{
    return m_fd;
}

}  // namespace coro

#endif  // CORO_CHANNEL_H
