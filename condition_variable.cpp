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
#include "condition_variable.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "mutex.h"
#include "spinlock.h"
#include "util.h"
#include "worker.h"

namespace ums {

Condition_variable::~Condition_variable() noexcept
{
    notify_all();
}

void Condition_variable::wait(std::unique_lock<Mutex>& lock)
{
    wait_internal(lock, Time_point::max());
}

void Condition_variable::notify_one() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    if (!m_waiters.empty())
        m_waiters.front()->notify_waiter();
}

void Condition_variable::notify_all() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    for (auto&& waiter : m_waiters)
        waiter->notify_waiter();
}

void Condition_variable::add_waiter()
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    m_waiters.push_back(this_worker);
}

// Since notify_all wakes up all waiters, we don't know
// which one will be first awaken, so we must call std::erase
// to remove worker, instead of pop_front.
//
void Condition_variable::remove_waiter()
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    std::erase(m_waiters, this_worker);
}

void Condition_variable::wait_internal(std::unique_lock<Mutex>& lock, const Time_point& abs_time)
{
    add_waiter();

    {
        this_worker->set_wait_info(false, abs_time);
        const Scoped_unlock<std::unique_lock<Mutex>> unlock{lock};
        this_worker->wait_cond_or_sleep();
    }

    remove_waiter();
}

std::cv_status Condition_variable::wait_until_internal(std::unique_lock<Mutex>& lock,
                                                       const Time_point& abs_time)
{
    wait_internal(lock, abs_time);
    return now() >= abs_time ? std::cv_status::timeout : std::cv_status::no_timeout;
}

Condition_variable_any::Condition_variable_any() : m_mtx{std::make_shared<Mutex>()} {}

void Condition_variable_any::notify_one() noexcept
{
    {
        const std::lock_guard<Mutex> lock{*m_mtx};
    }

    m_cv.notify_one();
}

void Condition_variable_any::notify_all() noexcept
{
    {
        const std::lock_guard<Mutex> lock{*m_mtx};
    }

    m_cv.notify_all();
}

} // namespace ums
