#pragma once

#ifndef COS_WORKER_H
#define COS_WORKER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "io_api.h"
#include "os_specific.h"
#include "task_manager.h"
#include "util.h"

class Scheduler;

class Worker final {
public:
    enum class State : int { initializing, idle, waiting, pending_io, runnable, running, exiting };

    Worker(uint64_t id, Scheduler& scheduler);

    ~Worker();

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    Worker(Worker&&) noexcept = delete;
    Worker& operator=(Worker&&) = delete;

    void entry_point();

    void main_loop();

    void yield();
    void read_file(void* file_handle, IO_Buffer buffer, uint64_t offset);
    void write_file(void* file_handle, IO_Buffer buffer, uint64_t offset);
    void wait_cond_or_sleep();

    template<class Rep, class Period>
    void sleep_for(const std::chrono::duration<Rep, Period>& time_to_sleep)
    {
        set_wait_info(false, now() + time_to_sleep);
        wait_cond_or_sleep();
    }

    void set_state(State state);

    void notify(std::unique_lock<std::mutex>& lock);
    void wait(std::unique_lock<std::mutex>& lock);

    [[nodiscard]] constexpr uint64_t id() const { return m_id; }

    [[nodiscard]] constexpr State state() const { return m_state; }

    [[nodiscard]] bool exit() const;

    // Helper struct used for info about waiting worker. Worker will sleep
    // until condition is set or until sleep time expires. This helper struct
    // is useful for mutex and condition variable, since there are functions which
    // requires waiting on a condition with some timeout.
    //
    struct Wait_info {
        std::atomic_flag m_cond{};
        Time_point m_sleep_time_point{Time_point::min()};
    };

    void set_cond() noexcept { m_wait_info.m_cond.test_and_set(std::memory_order_relaxed); };

    void clear_cond() noexcept { m_wait_info.m_cond.clear(std::memory_order_relaxed); };

    [[nodiscard]] bool check_cond() const noexcept
    {
        return m_wait_info.m_cond.test(std::memory_order_relaxed);
    };

    void set_sleep(Time_point sleep_time_point) noexcept
    {
        m_wait_info.m_sleep_time_point = sleep_time_point;
    }

    void clear_sleep() noexcept { m_wait_info.m_sleep_time_point = Time_point::min(); }

    [[nodiscard]] bool check_sleep() const noexcept
    {
        return m_wait_info.m_sleep_time_point != Time_point::min() &&
               m_wait_info.m_sleep_time_point <= now();
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

    // TODO: reorganize data members for quick access.
    //
    std::condition_variable m_cv;
    uint64_t m_id;
    State m_state;
    bool m_running; // Flag used for spurious wakeup check.
    Wait_info m_wait_info;
    std::shared_ptr<Task> m_task;
    std::unique_ptr<IO_Request> m_io_request;
    Scheduler& m_scheduler;
    std::thread m_thread;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local Worker* tls_worker;

#endif // COS_WORKER_H
