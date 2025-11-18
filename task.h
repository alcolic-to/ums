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

#ifndef UMS_TASK_H
#define UMS_TASK_H

#include <deque>
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
 * Generic task implementation.
 * It allows user to create task which has a generic return type (Task<u64> for example).
 * Task has a single storage composed of TaskBase, TaskResult and TaskExec.
 * This storage is shared between user and scheduler, and it's memory is managed automatically.
 * Task can also be obtained from async() call, which is a standard way of scheduling tasks
 * asynchronously.
 *
 * User can retrieve result from task with get() function, which will block current thread until
 * task is done. Also, user can call wait(), which will just block until task is done, but without
 * retieving result from task.
 */

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
 * parameters. It is used for task invokation.
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

    ReturnType execute()
    {
        return std::apply([](auto&& f, auto&&... args) { return f(args...); }, m_args);
    }

    void invoke() override
    {
        if constexpr (!std::is_same_v<ReturnType, void>)
            this->m_result = execute();
        else
            execute();
    };

    std::tuple<std::decay_t<Fn>, std::decay_t<Args>...> m_args;
};

/**
 * Simple wrapper class for user task.
 * It holds shared pointer to task storage for automatic memory management.
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

#endif // UMS_TASK_H
