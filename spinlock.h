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
#pragma once

#ifndef COS_SPINLOCK_H
#define COS_SPINLOCK_H

#include <atomic>
#include <cstdint>

namespace ums {

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

} // namespace ums

#endif // COS_SPINLOCK_H
