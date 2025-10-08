#pragma once

#ifndef COS_TASK_H
#define COS_TASK_H

#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "condition_variable.h"
#include "mutex.h"
#include "spinlock.h"
#include "types.h"
#include "util.h"

namespace ums {

/**
 * Task base class.
 * It hold all syncronization primitives shared between user and scheduler space.
 */
class TaskBase {
public:
    enum class State : int { not_started, running, done };

    TaskBase() = default;

    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&&) noexcept = delete;

    TaskBase& operator=(const TaskBase&) = delete;
    TaskBase& operator=(TaskBase&&) noexcept = delete;

    virtual ~TaskBase() = default;

    virtual void invoke() = 0;

    /**
     * Waits for task to be done.
     * It should be called from user space and scheduler will signal once task is done.
     */
    void wait()
    {
        std::unique_lock<Mutex> lock{m_mtx};
        m_cv.wait(lock, [&] { return m_state == TaskBase::State::done; });
    }

    /**
     * Notifies waiter in user space that task is done.
     */
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

/**
 * Task with result.
 * It is composed of TaskBase and it holds user's result and taken flag. User can get result only
 * once.
 */
template<class T>
class TaskResult : public TaskBase {
public:
    using ResultStorage = std::conditional_t<std::is_same_v<T, void>, u8, T>;

    /**
     * Extracts result from task storage and returns it.
     */
    T get()
    {
        if (m_taken)
            throw std::runtime_error{"Result already taken."};

        wait();
        m_taken = true;

        if constexpr (std::is_same_v<T, void>)
            return;
        else
            return std::move(m_result);
    }

    ResultStorage m_result;
    bool m_taken = false;
};

/**
 * Task execution class.
 * It is composed out of task with result and it contains user function along with provided
 * parameters. It is used for function invokation.
 */
template<class Fn, class... Args>
class TaskExec : public TaskResult<std::invoke_result_t<Fn, Args...>> {
public:
    using ReturnType = std::invoke_result_t<Fn, Args...>;

    explicit TaskExec(std::function<ReturnType(Args...)> function, Args&&... args) noexcept
        : m_func{std::move(function)}
        , m_args(std::forward<Args>(args)...)
    {
    }

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

/**
 * Simple wrapper class for user task.
 * It holds shared pointer to task storage in for automatic memory management.
 */
template<class T>
class Task {
public:
    explicit Task(std::shared_ptr<TaskResult<T>> task) noexcept : m_storage{std::move(task)} {}

    std::shared_ptr<TaskResult<T>> m_storage;

    TaskResult<T>* operator->() { return m_storage.get(); }
};

/**
 * Tasks queue which allows concurrent access.
 * Note that all memory operations can be relaxed, because spinlock has acquire-release memory
 * ordering.
 */
class alignas(cache_line_size) Tasks {
public:
    void enque(std::shared_ptr<TaskBase> task);
    std::shared_ptr<TaskBase> deque() noexcept;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    Spinlock m_lock;
    std::deque<std::shared_ptr<TaskBase>> m_tasks;
    std::atomic<sz> m_size{0};
};

} // namespace ums

#endif // COS_TASK_H
