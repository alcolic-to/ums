#pragma once

#ifndef COS_MUTEX_H
#define COS_MUTEX_H

#include <atomic>
#include <mutex>
#include <new>

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t cache_line_size = 64;
#endif

class alignas(cache_line_size) Spinlock {
public:
    void lock() noexcept;
    bool try_lock() noexcept;
    bool single_try_lock() noexcept;
    void unlock() noexcept;

private:
    std::atomic_flag m_lock;
};

class Mutex {
public:
    void lock();
    bool try_lock() noexcept;
    void unlock() noexcept;

private:
    Spinlock m_lock;
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
