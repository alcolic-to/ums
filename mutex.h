#pragma once

#ifndef COS_MUTEX_H
#define COS_MUTEX_H

#include <mutex>
#include <new>

#include "condition_variable.h"
#include "spinlock.h"
#include "util.h"

#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size;
#else
constexpr std::size_t cache_line_size = 64;
#endif

class Mutex;
class Recursive_mutex;

class Mutex_internal {
    friend class Mutex;
    friend class Recursive_mutex;

private:
    Mutex_internal() noexcept = default;

    void lock();
    bool try_lock() noexcept;
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
    bool try_lock() noexcept;
    void unlock() noexcept;

private:
    Mutex_internal m_mtx;
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
    bool try_lock() noexcept;
    void unlock() noexcept;

private:
    template<bool throws>
    bool inc_locks_count();

    [[nodiscard]] uint32_t dec_locks_count() noexcept;

    Mutex_internal m_mtx;
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
    bool try_lock() noexcept;

    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        return try_lock_until(now() + rel_time);
    }

    template<class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        return try_lock_until_internal(abs_time);
    }

    void unlock();

private:
    bool try_lock_until_internal(const Time_point& time_point);

    Mutex m_mtx;
    Condition_variable m_cv;
    bool m_locked{false};
};

// **** This is the initial mutex implementation which works much slower then current one.
// **** It uses waiting queue to wake up waiters. It is still here to avoid typing it again
// **** if we want to experiment with current implementation.
// **** It has additional members:
// **** Spinlock m_waiters_lock.
// **** std::vector<Worker*> m_waiters.
//
// Locks mutex.
// Note: tls_worker::clear_cond must be called before add_waiter in order to be synchronized with
// unlock.
//
// void Mutex::lock()
// {
//     if (try_lock())
//         return;

//     tls_worker->clear_cond();
//     add_waiter();

//     while (!m_lock.single_try_lock()) {
//         tls_worker->wait_condition();
//         tls_worker->clear_cond();
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
//     m_waiters.push_back(tls_worker);
// }

// void Mutex::remove_waiter() noexcept
// {
//     const std::scoped_lock<Spinlock> lock{m_waiters_lock};
//     std::erase(m_waiters, tls_worker);
// }

// void Mutex::notify_waiter() noexcept
// {
//     const std::scoped_lock<Spinlock> lock{m_waiters_lock};
//     if (!m_waiters.empty())
//         m_waiters.front()->set_cond();
// }

template<class Lockable>
class [[nodiscard]] scoped_unlock {
public:
    explicit scoped_unlock(Lockable& lockable) : m_lockable{lockable} { lockable.unlock(); }

    explicit scoped_unlock([[maybe_unused]] std::adopt_lock_t adopt, Lockable& lockable) noexcept
        : m_lockable{lockable}
    {
    }

    ~scoped_unlock() noexcept { m_lockable.lock(); }

    scoped_unlock(const scoped_unlock&) = delete;
    scoped_unlock& operator=(const scoped_unlock&) = delete;
    scoped_unlock(scoped_unlock&&) noexcept = delete;
    scoped_unlock& operator=(scoped_unlock&&) = delete;

private:
    Lockable& m_lockable;
};

#endif // COS_MUTEX_H
