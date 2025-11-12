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
#include "shared_mutex.h"

#include <mutex>

#include "mutex.h"
#include "util.h"

namespace ums {

// TODO: Check whether exception should be thrown in case of a deadlock.
// STL implementation does nothing in that case.
//
void Shared_mutex::lock()
{
    std::unique_lock<Mutex> lock{m_mutex};
    m_gate1.wait(lock, [&] { return !m_state.has_write(); });
    m_state.set_write();

    m_gate2.wait(lock, [&] { return !m_state.has_readers(); });
}

bool Shared_mutex::try_lock() noexcept
{
    const std::unique_lock<Mutex> lock{m_mutex};
    if (!m_state.has_readers() && !m_state.has_write()) {
        m_state.set_write();
        return true;
    }
    else
        return false;
}

void Shared_mutex::unlock() noexcept
{
    {
        const std::lock_guard<Mutex> lock{m_mutex};
        m_state.unset_write();
    }

    m_gate1.notify_all();
}

void Shared_mutex::lock_shared()
{
    std::unique_lock<Mutex> lock{m_mutex};
    m_gate1.wait(lock, [&] { return !m_state.has_write() && !m_state.max_readers(); });
    m_state.inc_readers();
}

bool Shared_mutex::try_lock_shared() noexcept
{
    const std::unique_lock<Mutex> lock{m_mutex};
    if (!m_state.has_write() && !m_state.max_readers()) {
        m_state.inc_readers();
        return true;
    }
    else
        return false;
}

void Shared_mutex::unlock_shared() noexcept
{
    bool max_readers = false;
    bool has_readers = false;
    bool has_write = false;

    {
        const std::lock_guard<Mutex> lock{m_mutex};
        max_readers = m_state.max_readers();
        m_state.dec_readers();
        has_readers = m_state.has_readers();
        has_write = m_state.has_write();
    }

    if (has_write) {
        if (!has_readers)
            m_gate2.notify_one();
    }
    else {
        if (max_readers)
            m_gate1.notify_one();
    }
}

bool Shared_timed_mutex::try_lock_until_internal(const Time_point& time_point)
{
    auto not_writing = [&] { return !m_state.has_write(); };
    auto no_readers = [&] { return !m_state.has_readers(); };

    std::unique_lock<Mutex> lock{m_mutex};
    if (!m_gate1.wait_until(lock, time_point, not_writing))
        return false;

    m_state.set_write();

    if (!m_gate2.wait_until(lock, time_point, no_readers)) {
        m_state.unset_write();
        lock.unlock();
        m_gate1.notify_all();
        return false;
    }

    return true;
}

bool Shared_timed_mutex::try_lock_shared_until_internal(const Time_point& time_point)
{
    auto cond = [&] { return !m_state.has_write() && !m_state.max_readers(); };

    std::unique_lock<Mutex> lock{m_mutex};
    if (!m_gate1.wait_until(lock, time_point, cond))
        return false;

    m_state.inc_readers();
    return true;
}

} // namespace ums
