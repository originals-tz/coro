#ifndef CORO_TASK_H
#define CORO_TASK_H

#include <event2/event.h>
#include <coroutine>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <variant>

namespace coro
{

struct Context
{
    event_base* m_base = nullptr;
    std::function<void()> m_destroy = []{};
};

template <typename T = void>
class Task;

struct PromiseBase
{
    struct FinalAwaiter
    {
        bool await_ready() const noexcept { return false; }

        template <typename promise_type>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> coroutine) noexcept
        {
            auto& promise = coroutine.promise();
            if (promise.m_continuation != nullptr)
            {
                return promise.m_continuation;
            }
            else
            {
                promise.m_ctx.lock()->m_destroy();
                return std::noop_coroutine();
            }
        }

        void await_resume() noexcept {}
    };

    std::suspend_always initial_suspend() noexcept { return {}; }

    FinalAwaiter final_suspend() noexcept { return {}; }

    void SetContinuation(std::coroutine_handle<> continuation) noexcept { m_continuation = continuation; }

    void SetContext(std::weak_ptr<Context> ctx) { m_ctx = std::move(ctx); }

    std::weak_ptr<Context> GetContext() { return m_ctx; }

protected:
    std::coroutine_handle<> m_continuation{nullptr};
    std::weak_ptr<Context> m_ctx;
};

template <typename return_type>
class Promise : public PromiseBase
{
    using task_type = Task<return_type>;
    using coroutine_handle = std::coroutine_handle<Promise<return_type>>;
    static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;
    using stored_type = std::conditional_t<return_type_is_reference, std::remove_reference_t<return_type>*, std::remove_const_t<return_type>>;
    using variant_type = std::variant<stored_type, std::exception_ptr>;

public:
    Promise(const Promise&) = delete;
    Promise(Promise&& other) = delete;
    Promise& operator=(const Promise&) = delete;
    Promise& operator=(Promise&& other) = delete;

    Promise() noexcept = default;
    ~Promise() = default;

    task_type get_return_object() noexcept;

    template <typename value_type>
        requires(return_type_is_reference and std::is_constructible_v<return_type, value_type &&>) or (not return_type_is_reference and std::is_constructible_v<stored_type, value_type &&>)
    void return_value(value_type&& value)
    {
        if constexpr (return_type_is_reference)
        {
            return_type ref = static_cast<value_type&&>(value);
            m_storage.template emplace<stored_type>(std::addressof(ref));
        }
        else
        {
            m_storage.template emplace<stored_type>(std::forward<value_type>(value));
        }
    }

    void return_value(stored_type value)
        requires(not return_type_is_reference)
    {
        if constexpr (std::is_move_constructible_v<stored_type>)
        {
            m_storage.template emplace<stored_type>(std::move(value));
        }
        else
        {
            m_storage.template emplace<stored_type>(value);
        }
    }

    void unhandled_exception() noexcept { new (&m_storage) variant_type(std::current_exception()); }

    auto result() & -> decltype(auto)
    {
        if (std::holds_alternative<stored_type>(m_storage))
        {
            if constexpr (return_type_is_reference)
            {
                return static_cast<return_type>(*std::get<stored_type>(m_storage));
            }
            else
            {
                return static_cast<const return_type&>(std::get<stored_type>(m_storage));
            }
        }
        else if (std::holds_alternative<std::exception_ptr>(m_storage))
        {
            std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
        }
        else
        {
            throw std::runtime_error{"The return value was never set, did you execute the coroutine?"};
        }
    }

    auto result() const& -> decltype(auto)
    {
        if (std::holds_alternative<stored_type>(m_storage))
        {
            if constexpr (return_type_is_reference)
            {
                return static_cast<std::add_const_t<return_type>>(*std::get<stored_type>(m_storage));
            }
            else
            {
                return static_cast<const return_type&>(std::get<stored_type>(m_storage));
            }
        }
        else if (std::holds_alternative<std::exception_ptr>(m_storage))
        {
            std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
        }
        else
        {
            throw std::runtime_error{"The return value was never set, did you execute the coroutine?"};
        }
    }

    auto result() && -> decltype(auto)
    {
        if (std::holds_alternative<stored_type>(m_storage))
        {
            if constexpr (return_type_is_reference)
            {
                return static_cast<return_type>(*std::get<stored_type>(m_storage));
            }
            else if constexpr (std::is_move_constructible_v<return_type>)
            {
                return static_cast<return_type&&>(std::get<stored_type>(m_storage));
            }
            else
            {
                return static_cast<const return_type&&>(std::get<stored_type>(m_storage));
            }
        }
        else if (std::holds_alternative<std::exception_ptr>(m_storage))
        {
            std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
        }
        else
        {
            throw std::runtime_error{"The return value was never set, did you execute the coroutine?"};
        }
    }

private:
    variant_type m_storage{};
};

template <>
class Promise<void> : public PromiseBase
{
    using task_type = Task<void>;
    using coroutine_handle = std::coroutine_handle<Promise<void>>;

public:
    Promise(const Promise&) = delete;
    Promise(Promise&& other) = delete;
    Promise& operator=(const Promise&) = delete;
    Promise& operator=(Promise&& other) = delete;

    Promise() noexcept = default;
    ~Promise() = default;

    task_type get_return_object() noexcept;

    void return_void() noexcept {}

    void unhandled_exception() noexcept { m_exception_ptr = std::current_exception(); }

    void result()
    {
        if (m_exception_ptr)
        {
            std::rethrow_exception(m_exception_ptr);
        }
    }

private:
    std::exception_ptr m_exception_ptr{nullptr};
};

template <typename return_type>
class Task
{
public:
    using task_type = Task<return_type>;
    using promise_type = Promise<return_type>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    struct awaitable_base
    {
        awaitable_base(coroutine_handle coroutine) noexcept
            : m_coroutine(coroutine)
        {}

        bool await_ready() const noexcept { return !m_coroutine || m_coroutine.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
        {
            m_coroutine.promise().SetContinuation(awaiting_coroutine);
            return m_coroutine;
        }

        template <class T>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<coro::Promise<T>> awaiting_coroutine) noexcept
        {
            m_coroutine.promise().SetContinuation(awaiting_coroutine);
            m_coroutine.promise().SetContext(awaiting_coroutine.promise().GetContext());
            return m_coroutine;
        }

        std::coroutine_handle<promise_type> m_coroutine{nullptr};
    };

    Task(const Task&) = delete;
    auto operator=(const Task&) -> Task& = delete;

    Task() noexcept
        : m_coroutine(nullptr)
        , m_ctx(std::make_shared<Context>())
    {}

    explicit Task(coroutine_handle handle)
        : m_coroutine(handle)
        , m_ctx(std::make_shared<Context>())
    {}

    Task(Task&& other) noexcept
        : m_coroutine(std::exchange(other.m_coroutine, nullptr))
        , m_ctx(std::exchange(other.m_ctx, nullptr))
    {}

    ~Task()
    {
        if (m_coroutine != nullptr)
        {
            m_coroutine.destroy();
        }
    }

    Task& operator=(Task&& other) noexcept
    {
        if (std::addressof(other) != this)
        {
            if (m_coroutine != nullptr)
            {
                m_coroutine.destroy();
            }

            m_coroutine = std::exchange(other.m_coroutine, nullptr);
        }

        return *this;
    }

    bool is_ready() const noexcept { return m_coroutine == nullptr || m_coroutine.done(); }

    bool resume()
    {
        if (!m_coroutine.done())
        {
            m_coroutine.resume();
        }
        return !m_coroutine.done();
    }

    bool destroy()
    {
        if (m_coroutine != nullptr)
        {
            m_coroutine.destroy();
            m_coroutine = nullptr;
            return true;
        }

        return false;
    }

    auto operator co_await() const& noexcept
    {
        struct awaitable : public awaitable_base
        {
            auto await_resume() { return this->m_coroutine.promise().result(); }
        };

        return awaitable{m_coroutine};
    }

    auto operator co_await() const&& noexcept
    {
        struct awaitable : public awaitable_base
        {
            auto await_resume() { return std::move(this->m_coroutine.promise()).result(); }
        };

        return awaitable{m_coroutine};
    }

    auto promise() & -> promise_type& { return m_coroutine.promise(); }
    auto promise() const& -> const promise_type& { return m_coroutine.promise(); }
    auto promise() && -> promise_type&& { return std::move(m_coroutine.promise()); }

    auto handle() -> coroutine_handle { return m_coroutine; }
    std::shared_ptr<Context> GetContext() { return m_ctx; }
    void SetEventBase(event_base* base) { m_ctx->m_base = base; }
    void SetDestroy(std::function<void()> destroy) { m_ctx->m_destroy = std::move(destroy); }

private:
    coroutine_handle m_coroutine{nullptr};
    std::shared_ptr<Context> m_ctx;
};

template <typename return_type>
inline auto Promise<return_type>::get_return_object() noexcept -> Task<return_type>
{
    auto t = Task<return_type>{coroutine_handle::from_promise(*this)};
    m_ctx = t.GetContext();
    return t;
}

inline auto Promise<void>::get_return_object() noexcept -> Task<>
{
    auto t = Task<>{coroutine_handle::from_promise(*this)};
    m_ctx = t.GetContext();
    return t;
}

}  // namespace coro

#endif  // CORO_TASK_H