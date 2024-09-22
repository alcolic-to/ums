#include "mutex.h"

#include <atomic>
#include <cstdint>

#include "worker.h"

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

// Acquires lock. Since C++20, it is possible to update atomic_flag's
// value only when there is a chance to acquire the lock,
// so we will spin on lock::test instead of lock::test_and_set.
// This will give us some performance improvements.
//
void Spinlock::lock() noexcept
{
    constexpr uint_fast32_t max_backoff = 64;
    uint_fast32_t backoff = 1;

    while (m_lock.test_and_set(std::memory_order_acquire)) {
        while (m_lock.test(std::memory_order_relaxed)) {
            for (uint_fast32_t i = 0; i < backoff; ++i)
                cpu_pause();

            backoff = backoff < max_backoff ? backoff << 1U : max_backoff;
        }
    }
}

bool Spinlock::try_lock() noexcept
{
    constexpr uint_fast32_t max_try = 256;
    uint_fast32_t try_count = 0;

    constexpr uint_fast32_t max_backoff = 64;
    uint_fast32_t backoff = 1;

    while (m_lock.test_and_set(std::memory_order_acquire)) {
        while (m_lock.test(std::memory_order_relaxed)) {
            if (++try_count == max_try)
                return false;

            for (uint_fast32_t i = 0; i < backoff; ++i)
                cpu_pause();

            backoff = backoff < max_backoff ? backoff << 1U : max_backoff;
        }
    }

    return true;
}

bool Spinlock::single_try_lock() noexcept
{
    return !m_lock.test_and_set(std::memory_order_acquire);
}

void Spinlock::unlock() noexcept
{
    m_lock.clear(std::memory_order_release);
}

void Mutex::lock()
{
    if (try_lock())
        return;

    while (!m_lock.single_try_lock())
        tls_worker->yield();
};

bool Mutex::try_lock() noexcept
{
    return m_lock.try_lock();
};

void Mutex::unlock() noexcept
{
    m_lock.unlock();
};
