/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef UMS_TASK_HPP
#define UMS_TASK_HPP

#include <deque>
#include <exception>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "condition_variable.hpp"
#include "mutex.hpp"
#include "spinlock.hpp"
#include "types.hpp"
#include "util.hpp"

namespace ums {

/**
 * FIXME: Implement proper exceptions for get(), wait() etc.
 */
enum class task_errc : u8 { no_state, result_already_retrived };

/**
 * Generic task implementation.
 * It allows user to create task which has a generic return type (Task<u64> for example).
 * Task has a single storage composed of TaskBase, TaskResult and TaskExec.
 * This storage is shared between user and scheduler, and it's memory is managed automatically.
 * Task can also be obtained from async() call, which is a standard way of scheduling tasks
 * asynchronously.
 *
 * User can retrieve result from task with get() function, which will block current thread until
 * task is done. Also, user can call wait(), which will just block until task is done, but
 * without retieving result from task.
 */

/**
 * Task base class.
 * It hold all syncronization primitives shared between user and scheduler space.
 */
class TaskBase {
public:
    enum State : u8 { init = 0, done = 1, taken = 2 };

    TaskBase() noexcept = default;

    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&&) noexcept = delete;

    TaskBase& operator=(const TaskBase&) = delete;
    TaskBase& operator=(TaskBase&&) noexcept = delete;

    virtual ~TaskBase() = default;

    void invoke_noexcept() noexcept
    {
        try {
            invoke();
        }
        catch (...) {
            this->set_exception(std::current_exception());
        }
    }

    /**
     * Waits for task to be done.
     * It should be called from user space and scheduler will signal once task is done.
     */
    void wait()
    {
        TTracyMessageLC(tracy_str("Waiting on task."), tracy::Color::Green1);

        std::unique_lock<Mutex> lock{m_mtx};
        m_cv.wait(lock, [&] { return is_state_done(); });

        TTracyMessageLC(tracy_str("Waiting on task - done."), tracy::Color::Green1);
    }

    /**
     * Notifies waiter in user space that task is done.
     * It should be called from scheduler space.
     */
    void notify()
    {
        TTracyMessageLC(tracy_str("Notifing on task."), tracy::Color::Green1);

        {
            const std::unique_lock<Mutex> lock{m_mtx};
            set_state_done();
        }

        m_cv.notify_one();
    }

protected:
    /**
     * All clases extending this one should override invoke().
     */
    virtual void invoke() = 0;

    /**
     * Called from extended classes to prepare getting out result.
     * Checks if result is already taken and handles exceptions logic.
     */
    void prepare_get()
    {
        if (is_state_taken())
            throw std::logic_error{"Result already taken."};

        wait();
        set_state_taken();

        if (has_exception())
            std::rethrow_exception(m_exception);
    }

private:
    bool is_state_done() const noexcept { return (m_state & State::done) != 0; }

    bool is_state_taken() const noexcept { return (m_state & State::taken) != 0; }

    bool has_exception() const noexcept { return m_exception != nullptr; }

    void set_state_done() noexcept { m_state = State(m_state | State::done); }

    void set_state_taken() noexcept { m_state = State(m_state | State::taken); }

    void set_exception(const std::exception_ptr& ex) noexcept { m_exception = ex; }

private: // NOLINT
    Mutex m_mtx;
    Condition_variable m_cv;
    std::exception_ptr m_exception;
    State m_state{State::init};
};

/**
 * Task with result.
 * It extends TaskBase and holds user's result (storage).
 */
template<class T>
class TaskResult : public TaskBase { // NOLINT (cppcoreguidelines-pro-type-member-init)
public:
    /**
     * Prepares and extracts result from task storage and returns it.
     */
    T get()
    {
        this->prepare_get();
        return std::move(*std::launder(std::bit_cast<T*>(std::addressof(m_storage))));
    }

protected:
    alignas(T) std::byte m_storage[sizeof(T)]; // NOLINT (hicpp-avoid-c-arrays)
};

/**
 * Void task result specialization.
 * It has no storage and get() just calls BaseTask::prepare_get().
 */
template<>
class TaskResult<void> : public TaskBase {
public:
    void get() { this->prepare_get(); }
};

/**
 * Task execution class.
 * It is composed out of TaskResult and it contains user function along with provided parameters. It
 * is used for task invokation.
 */
template<class Fn, class... Args>
class TaskExec : public TaskResult<std::invoke_result_t<Fn, Args...>> {
public:
    using ReturnType = std::invoke_result_t<Fn, Args...>;

    template<class F, class... A>
    explicit TaskExec(F&& func, A&&... args) noexcept
        : m_args{std::forward<F>(func), std::forward<A>(args)...}
    {
    }

protected:
    /**
     * Invokes user function and if return type is not void, emplace constructs return type in a
     * storage.
     */
    void invoke() override
    {
        if constexpr (!std::is_same_v<ReturnType, void>)
            new (std::addressof(this->m_storage))
                ReturnType{std::apply([](auto&& f, auto&&... args) { return f(args...); }, m_args)};
        else
            std::apply([](auto&& f, auto&&... args) { return f(args...); }, m_args);
    }

private:
    std::tuple<std::decay_t<Fn>, std::decay_t<Args>...> m_args;
};

/**
 * Simple wrapper class for user task.
 * It holds shared pointer to task storage for automatic memory management.
 */
template<class T>
class Task {
public:
    Task() noexcept = default;
    ~Task() = default;

    explicit Task(std::shared_ptr<TaskResult<T>> task) noexcept : m_storage{std::move(task)} {}

    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    Task(Task&& other) noexcept = default;
    Task& operator=(Task&& other) noexcept = default;

    [[nodiscard]] bool valid() const noexcept { return m_storage.operator bool(); }

    void wait()
    {
        if (!valid())
            throw std::logic_error{"Invalid task."};

        return m_storage->wait();
    }

    T get()
    {
        if (!valid())
            throw std::logic_error{"Invalid task."};

        return m_storage->get();
    }

private:
    std::shared_ptr<TaskResult<T>> m_storage;
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

    [[nodiscard]] usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    Spinlock m_lock;
    std::deque<std::shared_ptr<TaskBase>> m_tasks;
    std::atomic<usize> m_size{0};
};

} // namespace ums

#endif // UMS_TASK_HPP
