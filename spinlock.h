#pragma once

#ifndef COS_SPINLOCK_H
#define COS_SPINLOCK_H

#include <atomic>
#include <cstdint>

class Spinlock {
public:
    Spinlock() noexcept = default;
    ~Spinlock() noexcept = default;

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    Spinlock(Spinlock&&) noexcept = delete;
    Spinlock& operator=(Spinlock&&) = delete;

    void lock() noexcept;
    bool try_lock() noexcept;
    bool lock_with_timeout() noexcept;
    void unlock() noexcept;

private:
    std::atomic<uint32_t> m_flag{0};
};

#endif // COS_SPINLOCK_H
