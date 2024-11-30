#include "mutex.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <system_error>
#include <thread>

#include "spinlock.h"
#include "util.h"
#include "worker.h"

void Plain_mutex::lock()
{
    if (!m_spinlock.lock_with_timeout())
        while (!m_spinlock.try_lock())
            tls_worker->yield();
}

bool Plain_mutex::try_lock() noexcept
{
    return m_spinlock.try_lock();
}

void Plain_mutex::unlock() noexcept
{
    m_spinlock.unlock();
}

const std::thread::id empty_tid{};

void Mutex::lock()
{
    const auto this_tid{std::this_thread::get_id()};

    if (m_tid.load(std::memory_order_relaxed) == this_tid)
        throw std::system_error{std::make_error_code(std::errc::resource_deadlock_would_occur)};

    m_mtx.lock();

    m_tid.store(this_tid, std::memory_order_relaxed);
}

bool Mutex::try_lock() noexcept
{
    return m_mtx.try_lock();
}

void Mutex::unlock() noexcept
{
    m_tid.store(empty_tid, std::memory_order_relaxed);
    m_mtx.unlock();
}

// Helper function that increases locks count for recursive mutexes.
// If max_rec_locks is reched and throws is provided, throws resource_unavailable_try_again.
// Otherwise, returns whether number is incresed successfully.
//
template<bool throws>
bool rmtx_inc_lc(uint32_t& locks_count)
{
    ++locks_count;

    if (locks_count == max_rec_locks) [[unlikely]] {
        --locks_count;

        if constexpr (throws)
            throw std::system_error{
                std::make_error_code(std::errc::resource_unavailable_try_again)};

        return false;
    }
    else
        return true;
}

// Helper function that decreases locks count for recursive mutexes.
//
[[nodiscard]] uint32_t rmtx_dec_lc(uint32_t& locks_count) noexcept
{
    return --locks_count;
}

void Recursive_mutex::lock()
{
    const auto this_tid{std::this_thread::get_id()};

    if (m_tid.load(std::memory_order_relaxed) == this_tid) {
        rmtx_inc_lc<true>(m_locks_count);
        return;
    }

    m_mtx.lock();

    m_tid.store(this_tid, std::memory_order_relaxed);
    rmtx_inc_lc<false>(m_locks_count);
};

// Tries to lock mutex. It returns true if lock is acquired and lock count is incremented
// successfully, false otherwise.
//
bool Recursive_mutex::try_lock() noexcept
{
    const auto this_tid{std::this_thread::get_id()};

    if (m_tid.load(std::memory_order_relaxed) == this_tid)
        return rmtx_inc_lc<false>(m_locks_count);

    if (m_mtx.try_lock()) {
        rmtx_inc_lc<false>(m_locks_count);
        m_tid.store(this_tid, std::memory_order_relaxed);
        return true;
    }

    return false;
};

void Recursive_mutex::unlock() noexcept
{
    if (rmtx_dec_lc(m_locks_count) == 0) {
        m_tid.store(empty_tid, std::memory_order_relaxed);
        m_mtx.unlock();
    }
};

// TODO: I am too lazy to implement deadlock detection here.
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
    const std::unique_lock<Mutex> lock{m_mtx, std::try_to_lock};
    if (lock.owns_lock() && !m_locked)
        return m_locked = true;
    else
        return false;
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

void Timed_mutex::unlock()
{
    {
        const std::lock_guard<Mutex> lock{m_mtx};
        m_locked = false;
    }

    m_cv.notify_one();
}

void Recursive_timed_mutex::lock()
{
    const std::thread::id this_tid{std::this_thread::get_id()};
    std::unique_lock<Mutex> lock{m_mtx};

    if (m_tid != this_tid) {
        m_cv.wait(lock, [&] { return m_locks_count == 0; });
        m_tid = this_tid;
    }

    rmtx_inc_lc<true>(m_locks_count);
}

bool Recursive_timed_mutex::try_lock() noexcept
{
    const std::thread::id this_tid{std::this_thread::get_id()};
    const std::unique_lock<Mutex> lock{m_mtx, std::try_to_lock};

    if (lock.owns_lock() && (m_locks_count == 0 || m_tid == this_tid)) {
        if (rmtx_inc_lc<false>(m_locks_count)) {
            m_tid = this_tid;
            return true;
        }
    }

    return false;
}

bool Recursive_timed_mutex::try_lock_until_internal(const Time_point& time_point)
{
    const std::thread::id this_tid{std::this_thread::get_id()};
    std::unique_lock<Mutex> lock{m_mtx};

    if (m_tid == this_tid)
        return rmtx_inc_lc<false>(m_locks_count);

    m_cv.wait_until(lock, time_point, [&] { return m_locks_count == 0; });

    if (m_locks_count == 0) {
        rmtx_inc_lc<false>(m_locks_count);
        m_tid = this_tid;
        return true;
    }

    return false;
}

void Recursive_timed_mutex::unlock()
{
    std::unique_lock<Mutex> lock{m_mtx};
    if (rmtx_dec_lc(m_locks_count) == 0) {
        m_tid = empty_tid;
        lock.unlock();
        m_cv.notify_one();
    }
}
