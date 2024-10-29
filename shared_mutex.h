#pragma once

#ifndef COS_SHARED_MUTEX_H
#define COS_SHARED_MUTEX_H

#include "condition_variable.h"
#include "mutex.h"

// Shared mutex state that holds information for number of active readers (shared)
// and whether write is requested (unique).
//
class Shared_mutex_state {
    // clang-format off

    static constexpr uint32_t readers_bits  = 0b01111111'11111111'11111111'11111111U;
    static constexpr uint32_t write_bits    = 0b10000000'00000000'00000000'00000000U;

    static constexpr uint32_t readers_offset  = 0U;
    static constexpr uint32_t write_offset    = 31U;

    [[nodiscard]] uint32_t readers() const noexcept { return (m_state & readers_bits) >> readers_offset; }
    [[nodiscard]] uint32_t write() const noexcept { return (m_state & write_bits) >> write_offset; }

    // clang-format on
public:
    void unset_read() noexcept { m_state &= ~readers_bits; }

    [[nodiscard]] uint32_t readers_count() const noexcept { return readers(); }

    [[nodiscard]] bool has_readers() const noexcept { return bool(readers()); }

    void inc_readers() noexcept { ++m_state; }

    void dec_readers() noexcept { --m_state; }

    [[nodiscard]] bool max_readers() const noexcept
    {
        return (m_state & readers_bits) == readers_bits;
    }

    [[nodiscard]] bool has_write() const noexcept { return bool(write()); }

    void set_write() noexcept { m_state |= write_bits; }

    void unset_write() noexcept { m_state &= ~write_bits; }

    void clear() noexcept { m_state = 0U; }

private:
    uint32_t m_state{0U};
};

class Shared_timed_mutex;

// Shared mutex implementation from Howard Hinnant:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2406.html
//
class Shared_mutex {
    friend class Shared_timed_mutex;

public:
    Shared_mutex() noexcept = default;
    ~Shared_mutex() noexcept = default;

    Shared_mutex(const Shared_mutex&) = delete;
    Shared_mutex& operator=(const Shared_mutex&) = delete;

    Shared_mutex(Shared_mutex&&) noexcept = delete;
    Shared_mutex& operator=(Shared_mutex&&) = delete;

    void lock();
    bool try_lock() noexcept;
    void unlock() noexcept;

    void lock_shared();
    bool try_lock_shared() noexcept;
    void unlock_shared() noexcept;

private:
    Mutex m_mutex;
    Shared_mutex_state m_state;
    Condition_variable m_gate1;
    Condition_variable m_gate2;
};

class Shared_timed_mutex : public Shared_mutex {
public:
    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        return try_lock_until(now() + rel_time);
    }

    template<class Clock, class Duration>
    _NODISCARD_TRY_CHANGE_STATE bool
    try_lock_until(const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        return try_lock_until_internal(abs_time);
    }

    template<class Rep, class Period>
    _NODISCARD_TRY_CHANGE_STATE bool
    try_lock_shared_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        return try_lock_shared_until(now() + rel_time);
    }

    template<class Clock, class Duration>
    bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        return try_lock_shared_until_internal(abs_time);
    }

private:
    bool try_lock_until_internal(const Time_point& time_point);
    bool try_lock_shared_until_internal(const Time_point& time_point);
};

#endif // COS_SHARED_MUTEX_H
