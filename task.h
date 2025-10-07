#pragma once

#include <type_traits>
#ifndef COS_TASK_H
#define COS_TASK_H

#include <deque>
#include <functional>
#include <memory>
#include <tuple>

#include "condition_variable.h"
#include "mutex.h"
#include "spinlock.h"
#include "util.h"

namespace ums {

// class Task {
// public:
//     enum class State : int { not_started, running, done };

//     explicit Task(std::function<void()> function) noexcept;

//     void wait();
//     void notify() noexcept;
//     void operator()();

//     std::function<void()> m_func;
//     State m_state{State::not_started};
//     Mutex m_mtx;
//     Condition_variable m_cv;
// };

// // Tasks queue which allows concurrent access.
// // Note that all memory operations can be relaxed, because spinlock has acquire-release memory
// // ordering.
// //
// class alignas(cache_line_size) Tasks {
// public:
//     void enque(std::shared_ptr<Task> task);
//     std::shared_ptr<Task> deque() noexcept;

//     [[nodiscard]] size_t size() const noexcept;
//     [[nodiscard]] bool empty() const noexcept;

// private:
//     Spinlock m_lock;
//     std::deque<std::shared_ptr<Task>> m_tasks;
//     std::atomic<std::size_t> m_size{0};
// };

class TaskBase {
public:
    enum class State : int { not_started, running, done };

    TaskBase() = default;

    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&&) noexcept = delete;

    TaskBase& operator=(const TaskBase&) = delete;
    TaskBase& operator=(TaskBase&&) noexcept = delete;

    virtual ~TaskBase() {}

    virtual void invoke() = 0;

    void wait()
    {
        std::unique_lock<Mutex> lock{m_mtx};
        m_cv.wait(lock, [&] { return m_state == TaskBase::State::done; });
    }

    void notify() noexcept
    {
        {
            const std::unique_lock<Mutex> lock{m_mtx};
            m_state = State::done;
        }

        m_cv.notify_one();
    }

private:
    Mutex m_mtx;
    Condition_variable m_cv;
    State m_state{State::not_started};
};

template<class T, class... Args>
class Task : public TaskBase {
public:
    using ReturnType = std::conditional_t<std::is_same_v<T, void>, int, T>;

    explicit Task(std::function<T(Args...)> function, Args&&... args) noexcept
        : m_func{std::move(function)}
        , m_args(std::forward<Args>(args)...)
    {
    }

    void invoke() override
    {
        if constexpr (!std::is_same_v<T, void>)
            m_result = std::apply(m_func, m_args);
        else
            std::apply(m_func, m_args);
    };

    ReturnType result() { return m_result; }

    std::function<T(Args...)> m_func;
    std::tuple<Args...> m_args;
    ReturnType m_result;
};

// Tasks queue which allows concurrent access.
// Note that all memory operations can be relaxed, because spinlock has acquire-release memory
// ordering.
//
class alignas(cache_line_size) Tasks {
public:
    void enque(std::shared_ptr<TaskBase> task);
    std::shared_ptr<TaskBase> deque() noexcept;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    Spinlock m_lock;
    std::deque<std::shared_ptr<TaskBase>> m_tasks;
    std::atomic<std::size_t> m_size{0};
};

// class ClassTBase {
// public:
//     virtual void invoke() = 0;
// };

// template<class T, class... Args>
// class ClassT : public ClassTBase {
// public:
//     ClassT(std::function<T(Args...)> f, Args... args)
//         : m_func{std::move(f)}
//         , m_args{std::forward<Args>(args)...}
//     {
//     }

//     void invoke() override { m_result = std::apply(m_func, m_args); }

//     std::function<T(Args...)> m_func;
//     std::tuple<Args...> m_args;
//     T m_result;
// };

// template<class T, class... Args, class Result = ClassT<std::invoke_result_t<T, Args...>,
// Args...>> Result async2(T&& t, Args&&... args)
// {
//     Result task{std::forward<T>(t), std::forward<Args>(args)...};
//     return task;

//     // return t(std::forward<Args>(args)...);

//     // std::function<T(Args...)> func = t;
//     // func(std::forward<Args>(args)...);
// }

} // namespace ums

#endif // COS_TASK_H
