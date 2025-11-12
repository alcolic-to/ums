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

#ifndef COS_CONDITION_VARIABLE_H
#define COS_CONDITION_VARIABLE_H

#include <condition_variable>
#include <mutex>

#include "mutex.h"

namespace ums {

class Condition_variable; // To suppress unused header warnings.

class Condition_variable_any {
public:
    Condition_variable_any();

    template<class Lock>
    void wait(Lock& lock)
    {
        const std::shared_ptr<Mutex> ptr{m_mtx};
        std::unique_lock<Mutex> unique_lock{*ptr};
        Scoped_unlock<Lock> unlock{lock};
        std::lock_guard<std::unique_lock<Mutex>> adopted_lock{unique_lock, std::adopt_lock};
        m_cv.wait(unique_lock);
    }

    template<class Lock, class Predicate>
    void wait(Lock& lock, Predicate pred)
    {
        while (!pred())
            wait(lock);
    }

    template<class Lock, class Clock, class Duration>
    std::cv_status wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time)
    {
        std::shared_ptr<Mutex> ptr{m_mtx};
        std::unique_lock<Mutex> unique_lock{*ptr};
        Scoped_unlock<Lock> unlock{lock};
        std::lock_guard<std::unique_lock<Mutex>> adopted_lock{unique_lock, std::adopt_lock};
        return m_cv.wait_until(unique_lock, abs_time);
    }

    template<class Lock, class Clock, class Duration, class Predicate>
    bool wait_until(Lock& lock, const std::chrono::time_point<Clock, Duration>& abs_time,
                    Predicate pred)
    {
        while (!pred())
            if (wait_until(lock, abs_time) == std::cv_status::timeout)
                return pred();

        return true;
    }

    template<class Lock, class Rep, class Period>
    std::cv_status wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time)
    {
        return wait_until(lock, std::chrono::steady_clock::now() + rel_time);
    }

    template<class Lock, class Rep, class Period, class Predicate>
    bool wait_for(Lock& lock, const std::chrono::duration<Rep, Period>& rel_time, Predicate pred)
    {
        return wait_until(lock, std::chrono::steady_clock::now() + rel_time, std::move(pred));
    }

    void notify_one() noexcept;
    void notify_all() noexcept;

private:
    Condition_variable m_cv;
    std::shared_ptr<Mutex> m_mtx;
};

// TODO: Implement notify_all_at_thread_exit.
//
void notify_all_at_thread_exit(Condition_variable& cv, std::unique_lock<Mutex> lock);

} // namespace ums

#endif // COS_CONDITION_VARIABLE_H