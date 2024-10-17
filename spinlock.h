#pragma once

#ifndef COS_SPINLOCK_H
#define COS_SPINLOCK_H

#include <atomic>
#include <cstdint>
#include <thread>

enum class lock_type : uint32_t { lock, try_lock, single_try_lock };
enum class lock_error : uint32_t { success, deadlock, timeout };

static_assert(std::atomic<std::thread::id>::is_always_lock_free);

class Spinlock {
public:
    Spinlock() noexcept;
    ~Spinlock() noexcept = default;

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    Spinlock(Spinlock&&) noexcept = delete;
    Spinlock& operator=(Spinlock&&) = delete;

    template<lock_type type = lock_type::lock>
    lock_error lock() noexcept;

    void unlock() noexcept;

private:
    std::atomic<std::thread::id> m_tid;
};

#endif // COS_SPINLOCK_H
