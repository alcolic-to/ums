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

#ifndef UMS_MUTEX_HPP
#define UMS_MUTEX_HPP

#include <condition_variable>
#include <mutex>
#include <thread>

#include "spinlock.hpp"
#include "util.hpp"

namespace ums {

class Mutex;
class Worker;

// Similar to all c++ libs, this one is also based on Hinnant's implementation:
// https://github.com/llvm-mirror/libcxx/blob/master/include/condition_variable
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2406.html
//
class Condition_variable {
public:
    Condition_variable() noexcept = default;
    ~Condition_variable() noexcept;

    Condition_variable(const Condition_variable&) = delete;
    Condition_variable& operator=(const Condition_variable&) = delete;

    Condition_variable(Condition_variable&&) noexcept = delete;
    Condition_variable& operator=(Condition_variable&&) = delete;

    void wait(std::unique_lock<Mutex>& lock);

    template<class Predicate>
    void wait(std::unique_lock<Mutex>& lock, Predicate pred)
    {
        while (!pred())
            wait(lock);
    }

    template<class Rep, class Period>
    std::cv_status wait_for(std::unique_lock<Mutex>& lock,
                            const std::chrono::duration<Rep, Period>& rel_time)
    {
        return wait_until(lock, now() + rel_time);
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(std::unique_lock<Mutex>& lock, const std::chrono::duration<Rep, Period>& rel_time,
                  Predicate pred)
    {
        return wait_until(lock, now() + rel_time, std::move(pred));
    }

    template<class Clock, class Duration>
    std::cv_status wait_until(std::unique_lock<Mutex>& lock,
                              const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        if (now() >= abs_time)
            return std::cv_status::timeout;
        else
            return wait_until_internal(lock, abs_time);
    }

    template<class Clock, class Duration, class Predicate>
    bool wait_until(std::unique_lock<Mutex>& lock,
                    const std::chrono::time_point<Clock, Duration>& abs_time, Predicate pred)
    {
        while (!pred())
            if (wait_until(lock, abs_time) == std::cv_status::timeout)
                return pred();

        return true;
    }

    void notify_one() noexcept;
    void notify_all() noexcept;

private:
    void wait_internal(std::unique_lock<Mutex>& lock, const Time_point& abs_time);
    std::cv_status wait_until_internal(std::unique_lock<Mutex>& lock, const Time_point& abs_time);

    void add_waiter();
    void remove_waiter();

    Spinlock m_waiters_lock;
    std::vector<Worker*> m_waiters;
};

class Plain_mutex {
    friend class Mutex;
    friend class Recursive_mutex;

private:
    Plain_mutex() noexcept = default;

    void lock();
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;

    Spinlock m_spinlock;
};

class Mutex {
public:
    Mutex() noexcept = default;
    ~Mutex() noexcept = default;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    Mutex(Mutex&&) noexcept = delete;
    Mutex& operator=(Mutex&&) = delete;

    void lock();
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;

private:
    Plain_mutex m_mtx;
    std::atomic<std::thread::id> m_tid;
};

constexpr uint32_t max_rec_locks = uint32_t(-1);

class Recursive_mutex {
public:
    Recursive_mutex() noexcept = default;
    ~Recursive_mutex() noexcept = default;

    Recursive_mutex(const Recursive_mutex&) = delete;
    Recursive_mutex& operator=(const Recursive_mutex&) = delete;

    Recursive_mutex(Recursive_mutex&&) noexcept = delete;
    Recursive_mutex& operator=(Recursive_mutex&&) = delete;

    void lock();
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;

private:
    Plain_mutex m_mtx;
    std::atomic<std::thread::id> m_tid;
    uint32_t m_locks_count{0};
};

class Timed_mutex {
public:
    Timed_mutex() noexcept = default;
    ~Timed_mutex() noexcept = default;

    Timed_mutex(const Timed_mutex&) = delete;
    Timed_mutex& operator=(const Timed_mutex&) = delete;

    Timed_mutex(Timed_mutex&&) noexcept = delete;
    Timed_mutex& operator=(Timed_mutex&&) = delete;

    void lock();
    [[nodiscard]] bool try_lock() noexcept;

    template<class Rep, class Period>
    [[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        return try_lock_until(now() + rel_time);
    }

    template<class Clock, class Duration>
    [[nodiscard]] bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        return try_lock_until_internal(abs_time);
    }

    void unlock();

private:
    [[nodiscard]] bool try_lock_until_internal(const Time_point& time_point);

    Mutex m_mtx;
    Condition_variable m_cv;
    bool m_locked{false};
};

class Recursive_timed_mutex {
public:
    Recursive_timed_mutex() noexcept = default;
    ~Recursive_timed_mutex() noexcept = default;

    Recursive_timed_mutex(const Recursive_timed_mutex&) = delete;
    Recursive_timed_mutex& operator=(const Recursive_timed_mutex&) = delete;

    Recursive_timed_mutex(Recursive_timed_mutex&&) noexcept = delete;
    Recursive_timed_mutex& operator=(Recursive_timed_mutex&&) = delete;

    void lock();
    [[nodiscard]] bool try_lock() noexcept;

    template<class Rep, class Period>
    [[nodiscard]] bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        return try_lock_until(now() + rel_time);
    }

    template<class Clock, class Duration>
    [[nodiscard]] bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        return try_lock_until_internal(abs_time);
    }

    void unlock();

private:
    [[nodiscard]] bool try_lock_until_internal(const Time_point& time_point);

    Mutex m_mtx;
    Condition_variable m_cv;
    std::thread::id m_tid;
    uint32_t m_locks_count{0};
};

// **** This is the initial mutex implementation which works much slower then current one.
// **** It uses waiting queue to wake up waiters. It is still here to avoid typing it again
// **** if we want to experiment with current implementation.
// **** It has additional members:
// **** Spinlock m_waiters_lock.
// **** std::vector<Worker*> m_waiters.
//
// Locks mutex.
// Note: this_worker::clear_cond must be called before add_waiter in order to be synchronized with
// unlock.
//
// void Mutex::lock()
// {
//     if (try_lock())
//         return;

//     this_worker->clear_cond();
//     add_waiter();

//     while (!m_lock.single_try_lock()) {
//         this_worker->wait_condition();
//         this_worker->clear_cond();
//     }

//     remove_waiter();
// };

// void Mutex::unlock() noexcept
// {
//     m_lock.unlock();
//     notify_waiter();
// };

// bool Mutex::try_lock() noexcept
// {
//     return m_lock.try_lock();
// };

// void Mutex::add_waiter()
// {
//     const std::scoped_lock<Spinlock> lock{m_waiters_lock};
//     m_waiters.push_back(this_worker);
// }

// void Mutex::remove_waiter() noexcept
// {
//     const std::scoped_lock<Spinlock> lock{m_waiters_lock};
//     std::erase(m_waiters, this_worker);
// }

// void Mutex::notify_waiter() noexcept
// {
//     const std::scoped_lock<Spinlock> lock{m_waiters_lock};
//     if (!m_waiters.empty())
//         m_waiters.front()->set_cond();
// }

template<class Lockable>
class [[nodiscard]] Scoped_unlock {
public:
    explicit Scoped_unlock(Lockable& lockable) : m_lockable{lockable} { lockable.unlock(); }

    explicit Scoped_unlock([[maybe_unused]] std::adopt_lock_t adopt, Lockable& lockable) noexcept
        : m_lockable{lockable}
    {
    }

    ~Scoped_unlock() noexcept { m_lockable.lock(); }

    Scoped_unlock(const Scoped_unlock&) = delete;
    Scoped_unlock& operator=(const Scoped_unlock&) = delete;
    Scoped_unlock(Scoped_unlock&&) noexcept = delete;
    Scoped_unlock& operator=(Scoped_unlock&&) = delete;

private:
    Lockable& m_lockable;
};

} // namespace ums

#endif // UMS_MUTEX_HPP
