#include "spinlock.h"

#include <atomic>
#include <cstdint>
#include <thread>

// Pause intrinsic used for spinlock optimization.
// From intel documentation on void _mm_pause(void):
// https://www.intel.com/content/www/us/en/docs/cpp-compiler/developer-guide-reference/2021-8/pause-intrinsic.html
// The pause intrinsic is used in spin-wait loops with the processors implementing dynamic
// execution (especially out-of-order execution). In the spin-wait loop, the pause intrinsic
// improves the speed at which the code detects the release of the lock and provides especially
// significant performance gain.
//
// The execution of the next instruction is delayed for an implementation-specific amount of time.
// The PAUSE instruction does not modify the architectural state. For dynamic scheduling, the PAUSE
// instruction reduces the penalty of exiting from the spin-loop.
//
void cpu_pause() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
    // On x86/x86-64, use the PAUSE instruction
    __builtin_ia32_pause();
#elif defined(_MSC_VER)
    // On MSVC (Windows), use the intrinsic for the pause instruction
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    // On ARM (both 32-bit and 64-bit), use a NOP instruction or equivalent
    asm volatile("yield" ::: "memory");
#else
    // For other platforms, fallback to yield
    std::this_thread::yield();
#endif
}

using mo = std::memory_order;
const std::thread::id empty_tid{};

inline bool CAS(std::atomic<std::thread::id>& tid, std::thread::id& expected,
                const std::thread::id desired) noexcept
{
    return tid.compare_exchange_weak(expected, desired, mo::acquire, mo::relaxed);
}

Spinlock::Spinlock() noexcept : m_tid{empty_tid} {}

// Acquires lock by setting current thread id to m_tid.
// In case of a deadlock returns lock_error::deadlock.
// NOTE: We can use Worker::id() instead of std::thread::id, but we won't be able to use
// spinlock in userspace where worker is not initialized (perf tests for example).
//
template<lock_type type>
lock_error Spinlock::lock() noexcept
{
    const std::thread::id tid{std::this_thread::get_id()};
    std::thread::id expected_tid{empty_tid};

    if (CAS(m_tid, expected_tid, tid))
        return lock_error::success;

    if (expected_tid == tid)
        return lock_error::deadlock;

    if constexpr (type == lock_type::single_try_lock)
        return lock_error::timeout;

    constexpr uint32_t max_try = 256;
    uint32_t try_count = 0; // NOLINT

    constexpr uint32_t max_backoff = 64;
    uint32_t backoff = 1;

    expected_tid = empty_tid;

    while (m_tid.load(mo::relaxed) != empty_tid || !CAS(m_tid, expected_tid, tid)) {
        if constexpr (type == lock_type::try_lock) {
            if (++try_count == max_try) [[unlikely]]
                return lock_error::timeout;
        }

        for (uint32_t i = 0; i < backoff; ++i)
            cpu_pause();

        backoff = backoff < max_backoff ? backoff << 1U : max_backoff;
        expected_tid = empty_tid;
    }

    return lock_error::success;
}

void Spinlock::unlock() noexcept
{
    m_tid.store(empty_tid, mo::release);
}

template lock_error Spinlock::lock<lock_type::lock>();
template lock_error Spinlock::lock<lock_type::try_lock>();
template lock_error Spinlock::lock<lock_type::single_try_lock>();
