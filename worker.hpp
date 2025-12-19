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

#ifndef UMS_WORKER_HPP
#define UMS_WORKER_HPP

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "intrusive_list.hpp"
#include "io.hpp"
#include "os_specific.hpp"
#include "types.hpp"
#include "util.hpp"

namespace ums {

class Scheduler;
class Condition_variable;
class TaskBase;

class Worker final {
    friend class Scheduler;
    friend class Condition_variable;

public:
    enum class State : u8 {
        initializing,
        idle,
        waiting,
        pending_io,
        yielded,
        runnable,
        running,
        exiting
    };

    Worker(u64 id, Scheduler* scheduler);

    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    Worker(Worker&&) noexcept = delete;
    Worker& operator=(Worker&&) = delete;

    void entry_point();

    void main_loop();

    void yield();
    void read_file(void* file_handle, IO_Buffer buffer, u64 offset);
    void write_file(void* file_handle, IO_Buffer buffer, u64 offset);
    void wait_cond_or_sleep();

    template<class Clock, class Duration>
    void sleep_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        sleep_until_internal(time_point);
    }

    template<class Rep, class Period>
    void sleep_for(const std::chrono::duration<Rep, Period>& time_to_sleep)
    {
        sleep_until(now() + time_to_sleep);
    }

    void set_state(State state) noexcept;

    void wait(std::unique_lock<std::mutex>& lock);
    void notify(const std::unique_lock<std::mutex>& lock) noexcept;

    [[nodiscard]] constexpr u64 id() const noexcept { return m_id; }

    [[nodiscard]] constexpr State state() const noexcept { return m_state; }

    [[nodiscard]] bool exit() const noexcept;

private:
    // Helper struct used for info about waiting worker. Worker will sleep
    // until condition is set or until sleep time expires. This helper struct
    // is useful for mutex and condition variable, since there are functions which
    // requires waiting on a condition with some timeout.
    //
    struct Wait_info {
        std::atomic<bool> m_cond{false};
        Time_point m_sleep_time_point{Time_point::max()};
    };

    void set_cond() noexcept { m_wait_info.m_cond.store(true, std::memory_order_relaxed); };

    void clear_cond() noexcept { m_wait_info.m_cond.store(false, std::memory_order_relaxed); };

    [[nodiscard]] bool check_cond() const noexcept
    {
        return m_wait_info.m_cond.load(std::memory_order_relaxed);
    };

    void set_sleep(Time_point sleep_time_point) noexcept
    {
        m_wait_info.m_sleep_time_point = sleep_time_point;
    }

    void clear_sleep() noexcept { m_wait_info.m_sleep_time_point = Time_point::max(); }

    [[nodiscard]] bool check_sleep() const noexcept
    {
        return now() >= m_wait_info.m_sleep_time_point;
    }

public:
    [[nodiscard]] Time_point sleep_time_point() const noexcept
    {
        return m_wait_info.m_sleep_time_point;
    }

    void set_wait_info(bool cond, Time_point sleep_time_point) noexcept
    {
        if (cond)
            set_cond();
        else
            clear_cond();

        set_sleep(sleep_time_point);
    }

    void clear_wait_info() noexcept
    {
        clear_cond();
        clear_sleep();
    }

    [[nodiscard]] bool check_wait_info() const noexcept { return check_cond() || check_sleep(); }

    void notify_waiter() noexcept;

    void sleep_until_internal(const Time_point& time_point);

private:
    // TODO: reorganize data members for quicker access.
    //
    u64 m_id;
    Scheduler* m_scheduler;
    stl::INode<Worker> m_node;
    stl::INode<Worker> m_waiter_node; // Used in Condition_variable waiters list.
    std::condition_variable m_cv;
    State m_state{State::initializing};
    bool m_running{true}; // Flag used for spurious wakeup check.
    Wait_info m_wait_info;
    std::shared_ptr<TaskBase> m_task{nullptr};
    std::unique_ptr<IO_Request> m_io_request{nullptr};
    std::thread m_thread;
};

/**
 * API similar to std::thread::
 */
namespace worker {

Worker* get() noexcept;

u64 get_id() noexcept;

void yield() noexcept;

template<class Clock, class Duration>
void sleep_until(const std::chrono::time_point<Clock, Duration>& abs_time)
{
    get()->sleep_until(abs_time);
}

template<class Rep, class Period>
void sleep_for(const std::chrono::duration<Rep, Period>& rel_time)
{
    get()->sleep_for(rel_time);
}

}; // namespace worker

} // namespace ums

#endif // UMS_WORKER_HPP
