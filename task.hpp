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

#include <cassert>
#include <deque>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "condition_variable.hpp"
#include "intrusive_list.hpp"
#include "mutex.hpp"
#include "spinlock.hpp"
#include "types.hpp"
#include "util.hpp"

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
class TaskBase : public SharedState {
    friend class Tasks;

public:
    enum class State : u8 { not_started, running, done };

    /**
     * Creating 2 refs: 1 for us (WorkerTask) and 1 for user (Task).
     */
    TaskBase() : SharedState(2){};
    ~TaskBase() override = default;

    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&&) noexcept = delete;

    TaskBase& operator=(const TaskBase&) = delete;
    TaskBase& operator=(TaskBase&&) noexcept = delete;

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
    void notify()
    {
        {
            const std::unique_lock<Mutex> lock{m_mtx};
            m_state = State::done;
        }

        m_cv.notify_one();
    }

protected:
    stl::INode<TaskBase> m_node;
    State m_state{State::not_started};

private:
    Mutex m_mtx;
    Condition_variable m_cv;
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

    ~TaskResult() override = default;

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

private:
    void on_zero_refs() override
    {
        /**
         * FIXME: Once we implement value construction on task exec done only (with std::byte
         * storage for result), implement something similar to commented line below.
         *
         * if (m_state == State::done)
         *     std::launder<T*>(std::bit_cast<T*>(std::addressof(m_result)))->~T();
         */

        delete this;
    }

protected:
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

    ~TaskExec() override = default;

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
 * Simple wrapper class for worker task.
 * It hold pointer to base task class and it calls task::decrease_refs on destruction which will
 * call on_zero_refs which will call destructor of whole task if there are no more refs.
 */
class WorkerTask {
public:
    WorkerTask() = default;

    explicit WorkerTask(TaskBase* task) noexcept : m_task{task} {}

    ~WorkerTask()
    {
        if (valid())
            m_task->decrease_refs();
    }

    WorkerTask(const WorkerTask&) = delete;
    WorkerTask& operator=(const WorkerTask&) = delete;

    WorkerTask& operator=(TaskBase* task)
    {
        reset();
        m_task = task;

        return *this;
    }

    WorkerTask(WorkerTask&&) noexcept = default;
    WorkerTask& operator=(WorkerTask&&) noexcept = default;

    [[nodiscard]] bool valid() const noexcept { return m_task != nullptr; }

    TaskBase* const& operator->() const noexcept
    {
        assert(valid());
        return m_task;
    }

    TaskBase*& operator->() noexcept
    {
        assert(valid());
        return m_task;
    }

    explicit operator bool() const noexcept { return valid(); }

    void reset()
    {
        if (!valid())
            return;

        m_task->decrease_refs();
        m_task = nullptr;
    }

private:
    TaskBase* m_task{nullptr};
};

/**
 * Simple wrapper class for user task.
 * It holds shared pointer to task storage for automatic memory management.
 */
template<class T>
class Task {
public:
    explicit Task(TaskResult<T>* task) noexcept : m_storage{std::move(task)} {}

    ~Task() { reset(); }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : m_storage(other.m_storage) { other.m_storage = nullptr; };

    Task& operator=(Task&& other) noexcept
    {
        reset();
        m_storage = other.m_storage;
        other.m_storage = nullptr;
    };

    TaskResult<T>* operator->() { return m_storage; }

    [[nodiscard]] bool valid() const noexcept { return m_storage != nullptr; }

    void reset()
    {
        if (!valid())
            return;

        m_storage->decrease_refs();
        m_storage = nullptr;
    }

private:
    TaskResult<T>* m_storage;
};

/**
 * Tasks queue which allows concurrent access.
 * Note that all memory operations can be relaxed, because spinlock has acquire-release memory
 * ordering.
 */
class alignas(cache_line_size) Tasks {
public:
    void enque(TaskBase& task);
    TaskBase* deque() noexcept;

    [[nodiscard]] usize size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    Spinlock m_lock;
    std::atomic<usize> m_size{0};
    // std::deque<std::shared_ptr<TaskBase>> m_tasks;

    stl::IList<TaskBase, &TaskBase::m_node> m_tasks;
};

} // namespace ums

#endif // UMS_TASK_HPP
