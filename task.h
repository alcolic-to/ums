#pragma once

#ifndef COS_TASK_H
#define COS_TASK_H

#include <deque>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

#include "condition_variable.h"
#include "mutex.h"
#include "spinlock.h"
#include "types.h"
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

class TaskBaseImpl {
public:
    enum class State : int { not_started, running, done };

    TaskBaseImpl() = default;

    TaskBaseImpl(const TaskBaseImpl&) = delete;
    TaskBaseImpl(TaskBaseImpl&&) noexcept = delete;

    TaskBaseImpl& operator=(const TaskBaseImpl&) = delete;
    TaskBaseImpl& operator=(TaskBaseImpl&&) noexcept = delete;

    virtual ~TaskBaseImpl() = default;

    virtual void invoke() = 0;

    void wait()
    {
        std::unique_lock<Mutex> lock{m_mtx};
        m_cv.wait(lock, [&] { return m_state == TaskBaseImpl::State::done; });
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

template<class T>
class TaskImpl : public TaskBaseImpl {
public:
    using ResultType = std::conditional_t<std::is_same_v<T, void>, u8, T>;

    // ~TaskImpl() override { std::cout << "Destorying Task!\n"; }

    ResultType result() { return m_result; }

    ResultType m_result;
};

template<class Fn, class... Args>
class TaskExecImpl : public TaskImpl<std::invoke_result_t<Fn, Args...>> {
public:
    using ReturnType = std::invoke_result_t<Fn, Args...>;

    explicit TaskExecImpl(std::function<ReturnType(Args...)> function, Args&&... args) noexcept
        : m_func{std::move(function)}
        , m_args(std::forward<Args>(args)...)
    {
    }

    // ~TaskExecImpl() override { std::cout << "Destorying TaskExec!\n"; }

    void invoke() override
    {
        if constexpr (!std::is_same_v<ReturnType, void>)
            this->m_result = std::apply(m_func, m_args);
        else
            std::apply(m_func, m_args);
    };

    std::function<ReturnType(Args...)> m_func;
    std::tuple<Args...> m_args;
};

class TaskBase {
public:
    std::shared_ptr<TaskBaseImpl> m_storage;

    TaskBaseImpl* operator->() { return m_storage.get(); }
};

template<class T>
class Task {
public:
    explicit Task(std::shared_ptr<TaskImpl<T>> task) noexcept : m_storage{std::move(task)} {}

    std::shared_ptr<TaskImpl<T>> m_storage;

    TaskImpl<T>* operator->() { return m_storage.get(); }
};

// Tasks queue which allows concurrent access.
// Note that all memory operations can be relaxed, because spinlock has acquire-release memory
// ordering.
//
class alignas(cache_line_size) Tasks {
public:
    void enque(std::shared_ptr<TaskBaseImpl> task);
    std::shared_ptr<TaskBaseImpl> deque() noexcept;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    Spinlock m_lock;
    std::deque<std::shared_ptr<TaskBaseImpl>> m_tasks;
    std::atomic<sz> m_size{0};
};

} // namespace ums

#endif // COS_TASK_H
