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

    template<lock_type lt>
    inline lock_error spin_lock() noexcept;

    template<class Mutex_class, bool throws = false>
    [[nodiscard]] inline bool
    check_lock(lock_error err) noexcept(std::is_same_v<Mutex_class, Recursive_mutex> || !throws);

    template<class Mutex_class>
    void lock();

    template<class Mutex_class>
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

// *************************
// New mutex implementation.
// *************************

// enum class Mutex_type : int { klot = 0b1 };

// // These statics shoud not be in *.h file.
// //
// class mutex_storage_wrapper {
//     static constexpr uint64_t count_bits =
//         0b00000000'00000000'00000000'00000000'00111111'11111111'11111111'11111111;
//     static constexpr uint64_t type_bits =
//         0b00000000'00000000'00000000'00000000'11000000'00000000'00000000'00000000;
//     static constexpr uint64_t tid_bits =
//         0b11111111'11111111'11111111'11111111'00000000'00000000'00000000'00000000;

//     static constexpr uint64_t count_offset = 0;
//     static constexpr uint64_t type_offset = 30;
//     static constexpr uint64_t tid_offset = 32;

// public:
//     explicit mutex_storage_wrapper(uint64_t flags = 0) : m_flags{flags} {};

// private:
//     uint64_t m_flags{0};
// };

// // '00000000'00000000'00000000'00000000'00 000000'00000000'00000000'00000000
// // |             thread_id             |  |             count              |
// //                                       type
// class mutex_storage {
// public:
//     mutex_storage() noexcept = default;
//     ~mutex_storage() noexcept = default;

//     mutex_storage(const mutex_storage&) = delete;
//     mutex_storage& operator=(const mutex_storage&) = delete;

//     mutex_storage(mutex_storage&&) noexcept = delete;
//     mutex_storage& operator=(mutex_storage&&) = delete;

// private:
//     std::atomic<uint64_t> flags{0};
// };

// All data representing a move is packed within a single int. Currently using 19 bits, in this
// order (from MSB to LSB):
//		4 bits - promotion piece type
//		3 bits - move type
//		6 bits - "to" square
//		6 bits - "from" square
//
// struct Move
// {
// 	Move() = default;
// 	constexpr Move(int flags) : m_flags(flags) {  }
// 	constexpr Move(Square from, Square to, MoveType mt) : m_flags(int(from) | (int(to) << 6) |
// (int(mt) << 12))
// 	{
// 		DBG_ASSERT(mt != MT_PROMOTION);
// 	}

// 	constexpr Move(Square from, Square to, PieceType promotionPieceType) : m_flags(int(from) |
// (int(to) << 6) | (int(MT_PROMOTION) << 12) | (int(promotionPieceType) << 15)) { }

// 	constexpr inline Square FromSquare() const { return Square(m_flags & 0b0000000000111111); }
// 	constexpr inline Square ToSquare() const { return Square((m_flags & 0b0000111111000000) >> 6); }
// 	constexpr inline MoveType Type() const { return MoveType((m_flags >> 12) & 0b111); }
// 	constexpr inline PieceType PromotionPieceType() const { return PieceType(m_flags >> 15); }
// 	constexpr inline bool IsCapture() const { return Type() == MT_CAPTURE || Type() ==
// MT_EN_PASSANT; } 	constexpr inline int FromToSquares() const { return int(m_flags &
// 0b0000111111111111); }

// 	// In order for the search to be more efficient, we sort the moves so that the captures are
// searched first - std::sort uses operator< by default for sorting the values.
// 	// Moves of type MT_CAPTURE will have largest m_flags, since move type bits are the MSBs (and
// MT_CAPTURE is the largest value in the move type enum).
// 	//
// 	constexpr inline bool operator<(const Move& m) const { return m_flags > m.m_flags; }
// 	constexpr inline bool operator==(const Move& m) const { return m_flags == m.m_flags; }
// 	constexpr inline bool operator!=(const Move& m) const { return !(*this == m); }

// 	constexpr explicit operator bool() const { return m_flags; }
// 	constexpr explicit operator int() const { return m_flags; }
// 	constexpr explicit operator uint32_t() const { return m_flags; }

// 	int m_flags;
// };

#endif // COS_MUTEX_H
