/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "spinlock.hpp"

#include <atomic>

#include "types.hpp"

namespace ums {

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

namespace {

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

} // anonymous namespace

using mo = std::memory_order;

// Plain spinlock implementation.
// It loads lock flag until it becames free (without acquiring hardware cache line lock),
// after which tries to atomically set flag to 1. If someone else has already
// set value to 1 (took lock), exchange returns 1 and we continue spinning.
// Instead of using CAS for setting flag, simple exchange is used which is ~30% faster.
// Also, for performance improvement, Intel optimization manual is followed: we use exponential
// backoff with pause instruction. Maximum backoff is taken based on microsoft STL spinlock
// implementation, and after testing it turs out that we are not getting better results with any
// other number. Memory bariers are standard acquire/release for lock/unlock pairs, but spinning on
// flag is done with relaxed atomic, because memory order is not important until lock is taken.
//
// TODO: We can add in debug builds all sorts of features (deadlock detection, time measurements,
// asserting on long lock held etc). We should also prevent yielding while lock is held. In that
// case there is no point in adding optimizations by checking which CPU holds the lock.
//
// TODO: I read in glibc that exchange on some architectures can use strong CAS,
// and if that is true, we should replace exchange with compare_exchange_weak.
// https://github.com/lattera/glibc/blame/master/nptl/pthread_spin_lock.c
// I checked ARM and it is fine. On power64, I am not sure what happens...
//
void Spinlock::lock() noexcept
{
    constexpr u32 max_backoff = 64;
    u32 backoff = 1;

    while (m_flag.load(mo::relaxed) != 0 || m_flag.exchange(1, mo::acquire) != 0) {
        for (u32 i = 0; i < backoff; ++i)
            cpu_pause();

        backoff = backoff < max_backoff ? backoff << 1U : max_backoff;
    }
}

bool Spinlock::try_lock() noexcept
{
    return m_flag.load(mo::relaxed) == 0 && m_flag.exchange(1, mo::acquire) == 0;
}

// Tries to acquire lock max_try number of times and returns whether it succeeded.
//
bool Spinlock::lock_with_timeout() noexcept
{
    constexpr u32 max_try = 256;
    u32 try_count = 0;

    constexpr u32 max_backoff = 64;
    u32 backoff = 1;

    while (m_flag.load(mo::relaxed) != 0 || m_flag.exchange(1, mo::acquire) != 0) {
        if (++try_count == max_try) [[unlikely]]
            return false;

        for (u32 i = 0; i < backoff; ++i)
            cpu_pause();

        backoff = backoff < max_backoff ? backoff << 1U : max_backoff;
    }

    return true;
}

void Spinlock::unlock() noexcept
{
    m_flag.store(0, mo::release);
}

} // namespace ums
