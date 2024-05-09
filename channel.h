#ifndef CORO_CHANNEL_H
#define CORO_CHANNEL_H

#include <mutex>
#include <optional>
#include <set>
#include "awaiter.h"
#include "eventfd.h"
#include "scheduler.h"
#include "task.h"

namespace coro
{

template <typename T>
class Channel
{
public:
    Channel()
        : m_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
    {}

    ~Channel() { close(m_fd); }

    /**
     * @brief 关闭channel，唤醒协程
     */
    void Close()
    {
        if (!m_is_close)
        {
            std::lock_guard lk(m_mut);
            m_is_close = true;
            eventfd_write(m_fd, 1);
            eventfd_write(m_select_fd, 1);
        }
    }

    /**
     * @brief 添加一个数据
     * @param t 数据
     * @return 添加成功后返回true
     */
    template <class DATA>
    bool Push(DATA&& t)
    {
        if (m_is_close)
        {
            return false;
        }
        std::lock_guard lk(m_mut);
        m_data_queue.emplace(std::forward<DATA>(t));
        m_data_count++;
        eventfd_write(m_fd, 1);
        eventfd_write(m_select_fd, 1);
        return true;
    }

    /**
     * @brief 获取一个数据
     * @param t 数据引用
     * @return 获取成功后返回true
     */
    Task<bool> Pop(T& t)
    {
        if (m_is_close)
        {
            co_return false;
        }

        bool s = false;
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
            co_await EventFdAwaiter(m_fd);
        } while (!m_is_close);
        //! 唤醒其他等待的协程退出
        eventfd_write(m_fd, 1);
        co_return s;
    }

    /**
     * @brief 尝试获取数据
     * @param t 数据引用
     * @return 获取成功后返回true
     */
    bool TryPop(T& t)
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

    /**
     * @brief 绑定select
     * @param fd select的fd
     * @return 绑定成功后返回true
     */
    bool BindSelect(int fd)
    {
        if (m_is_close)
        {
            return false;
        }
        std::lock_guard lk(m_mut);
        m_select_fd = fd;
        return true;
    }

    /**
     * @brief 判断channel是否关闭
     * @return channel关闭返回true
     */
    bool IsClose() { return m_is_close; }

    /**
     * @brief 判断数据队列是否为空
     * @return 数据队列为空返回true
     */
    bool IsEmpty() { return m_data_count == 0; }

private:
    std::atomic_size_t m_data_count = 0;
    //! 通知channel的fd
    int m_fd = -1;
    //! 通知select的fd
    int m_select_fd = -1;
    //! 可重入的互斥锁
    std::recursive_mutex m_mut;
    //! 数据队列
    std::queue<std::optional<T>> m_data_queue;
    //! 是否关闭
    std::atomic_bool m_is_close = false;
};

class Select
{
public:
    Select()
        : m_fd(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK))
    {}

    ~Select() { close(m_fd); }

    /**
     * @brief 等待某个channel的数据
     * @tparam CHANNEL channel类型
     * @param chan 多个channel
     * @return 某个channel关闭，返回false, 某个channel存在数据，返回true
     */
    template <typename... CHANNEL>
    coro::Task<bool> operator()(CHANNEL&&... chan)
    {
        if (!(... && chan.BindSelect(m_fd)))
        {
            co_return false;
        }
        if (!(... && chan.IsEmpty()))
        {
            co_return true;
        }
        co_await EventFdAwaiter(m_fd);
        co_return !(... && chan.IsClose());
    }

private:
    //! 用于等待的文件描述符
    int32_t m_fd = -1;
};
}  // namespace coro

#endif  // CORO_CHANNEL_H
