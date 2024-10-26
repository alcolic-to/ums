#include "mutex.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <system_error>
#include <thread>

#include "spinlock.h"
#include "util.h"
#include "worker.h"

void Mutex_internal::lock()
{
    if (!m_spinlock.lock_with_timeout())
        while (!m_spinlock.try_lock())
            tls_worker->yield();
}

bool Mutex_internal::try_lock() noexcept
{
    return m_spinlock.try_lock();
}

void Mutex_internal::unlock() noexcept
{
    m_spinlock.unlock();
}

const std::thread::id empty_tid{};

void Mutex::lock()
{
    const auto this_tid = std::this_thread::get_id();

    if (m_tid.load(std::memory_order_relaxed) == this_tid)
        throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));

    m_mtx.lock();

    m_tid.store(this_tid, std::memory_order_relaxed);
}

bool Mutex::try_lock() noexcept
{
    return m_mtx.try_lock();
}

void Mutex::unlock() noexcept
{
    m_mtx.unlock();
    m_tid.store(empty_tid, std::memory_order_relaxed);
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
    const auto this_tid = std::this_thread::get_id();

    if (m_tid.load(std::memory_order_relaxed) == this_tid) {
        inc_locks_count<true>();
        return;
    }

    m_mtx.lock();

    m_tid.store(this_tid, std::memory_order_relaxed);
    inc_locks_count<false>(); // Never throws since counter is 0 if we locked mutex.
};

// Tries to lock mutex. It returns true if lock is acquired and lock count is incremented
// successfully, false otherwise.
//
bool Recursive_mutex::try_lock() noexcept
{
    const auto this_tid = std::this_thread::get_id();

    if (m_tid.load(std::memory_order_relaxed) == this_tid)
        return inc_locks_count<false>();

    if (m_mtx.try_lock()) {
        inc_locks_count<false>();
        m_tid.store(this_tid, std::memory_order_relaxed);
        return true;
    }

    return false;
};

void Recursive_mutex::unlock() noexcept
{
    if (dec_locks_count() == 0) {
        m_mtx.unlock();
        m_tid.store(empty_tid, std::memory_order_relaxed);
    }
};

// I am too lazy to implement deadlock detection here.
// To implement it just save thread id of the thread holding mutex,
// and check it every time lock is called.
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
