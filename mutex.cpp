#include "mutex.h"

#include <cstdint>
#include <mutex>
#include <system_error>

#include "spinlock.h"
#include "util.h"
#include "worker.h"

template<lock_type type>
inline lock_error Mutex_internal::spin_lock() noexcept
{
    return m_spinlock.lock<type>();
}

// Helper function that checks spinlock error and converts it to bool value.
// For non-recursive mutex, returns true for success. If throws is set to
// true and deadlock occurs, throws resource_deadlock_would_occur, otherwise
// returns false.
// For recursive mutex, we will return true for success and deadlock, false otherwise.
//
template<class Mutex_class, bool throws>
[[nodiscard]] inline bool Mutex_internal::check_lock(const lock_error err) noexcept(
    std::is_same_v<Mutex_class, Recursive_mutex> || !throws)
{
    if (err == lock_error::success)
        return true;

    if (err == lock_error::deadlock) {
        if constexpr (std::is_same_v<Mutex_class, Recursive_mutex>)
            return true;
        else if constexpr (throws)
            throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));
    }

    return false;
}

template<class Mutex_class>
void Mutex_internal::lock()
{
    constexpr auto try_lock = lock_type::try_lock;
    constexpr auto single_try_lock = lock_type::single_try_lock;

    if (check_lock<Mutex_class, true>(spin_lock<try_lock>()))
        return;

    while (!check_lock<Mutex_class>(spin_lock<single_try_lock>()))
        tls_worker->yield();
}

// TODO: Check whether we should just single try lock.
//
template<class Mutex_class>
bool Mutex_internal::try_lock() noexcept
{
    constexpr auto try_lock = lock_type::try_lock;

    return check_lock<Mutex_class, false>(spin_lock<try_lock>());
}

// Since standard requires that unlock does not throw (it is UB), we can not throw
// operation_not_permitted if thread does not own the mutex.
//
void Mutex_internal::unlock() noexcept
{
    m_spinlock.unlock();
}

void Mutex::lock()
{
    m_mtx.lock<Mutex>();
}

bool Mutex::try_lock() noexcept
{
    return m_mtx.try_lock<Mutex>();
}

void Mutex::unlock() noexcept
{
    m_mtx.unlock();
}

// Increments locks count and returns if it succeded.
// If throws is specified, throws if maximum recursive locks are reached.
//
template<bool throws>
bool Recursive_mutex::inc_locks_count()
{
    ++m_locks_count;

    if (m_locks_count == max_rec_locks) [[unlikely]] {
        --m_locks_count;

        if constexpr (throws)
            throw std::system_error(
                std::make_error_code(std::errc::resource_unavailable_try_again));

        return false;
    }
    else
        return true;
}

[[nodiscard]] uint32_t Recursive_mutex::dec_locks_count() noexcept
{
    return --m_locks_count;
}

void Recursive_mutex::lock()
{
    m_mtx.lock<Recursive_mutex>();
    inc_locks_count<true>();
};

// Tries to lock mutex. It returns true if lock is acquired and lock count is incremented
// successfully, false otherwise.
//
bool Recursive_mutex::try_lock() noexcept
{
    return m_mtx.try_lock<Recursive_mutex>() && inc_locks_count<false>();
};

void Recursive_mutex::unlock() noexcept
{
    if (dec_locks_count() == 0)
        m_mtx.unlock();
};

// I am too lazy to implement deadlock detection here.
// To implement it just save thread id of the thread holding mutex,
// and check it every time lock is called.
// Another approach would be to implement it within mutex itself.
//
void Timed_mutex::lock()
{
    std::unique_lock<Mutex> lock{m_mtx};
    m_cv.wait(lock, [&] { return !m_locked; });
    m_locked = true;
}

bool Timed_mutex::try_lock() noexcept
{
    const std::unique_lock<Mutex> lk{m_mtx, std::try_to_lock};
    if (lk.owns_lock() && !m_locked)
        return m_locked = true;
    else
        return false;
}

void Timed_mutex::unlock()
{
    {
        const std::lock_guard<Mutex> lock{m_mtx};
        m_locked = false;
    }

    m_cv.notify_one();
}

bool Timed_mutex::try_lock_until_internal(const Time_point& time_point)
{
    std::unique_lock<Mutex> lock{m_mtx};
    m_cv.wait_until(lock, time_point, [&] { return !m_locked; });

    if (!m_locked)
        return m_locked = true;
    else
        return false;
}
