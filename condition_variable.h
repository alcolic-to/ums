#pragma once

#ifndef COS_CONDITION_VARIABLE_H
#define COS_CONDITION_VARIABLE_H

#include <condition_variable>
#include <mutex>
#include <vector>

#include "spinlock.h"
#include "util.h"

class Worker;
class Mutex;

class Condition_variable {
public:
    Condition_variable() noexcept = default;
    ~Condition_variable() noexcept;

    Condition_variable(const Condition_variable&) = delete;
    Condition_variable& operator=(const Condition_variable&) = delete;

    Condition_variable(Condition_variable&&) noexcept = delete;
    Condition_variable& operator=(Condition_variable&&) = delete;

    void wait(std::unique_lock<Mutex>& lock);

    template<class Predicate>
    void wait(std::unique_lock<Mutex>& lock, Predicate pred)
    {
        while (!pred())
            wait(lock);
    }

    template<class Rep, class Period>
    std::cv_status wait_for(std::unique_lock<Mutex>& lock,
                            const std::chrono::duration<Rep, Period>& time)
    {
        return wait_until(lock, now() + time);
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(std::unique_lock<Mutex>& lock, const std::chrono::duration<Rep, Period>& time,
                  Predicate pred)
    {
        return wait_until(lock, now() + time, std::forward<Predicate>(pred)); // or just std::move?
    }

    template<class Clock, class Duration>
    std::cv_status wait_until(std::unique_lock<Mutex>& lock,
                              const std::chrono::time_point<Clock, Duration>& time_point)
    {
        if (now() >= time_point)
            return std::cv_status::timeout;
        else
            return wait_until_internal(lock, time_point);
    }

    template<class Clock, class Duration, class Predicate>
    bool wait_until(std::unique_lock<Mutex>& lock,
                    const std::chrono::time_point<Clock, Duration>& time_point, Predicate pred)
    {
        while (!pred())
            if (wait_until(lock, time_point) == std::cv_status::timeout)
                return pred();

        return true;
    }

    void notify_one() noexcept;
    void notify_all() noexcept;

private:
    void wait_internal(std::unique_lock<Mutex>& lock, const Time_point& time_point);
    std::cv_status wait_until_internal(std::unique_lock<Mutex>& lock, const Time_point& time_point);

    void add_waiter();
    void remove_waiter();

    Spinlock m_waiters_lock;
    std::vector<Worker*> m_waiters;
};

#endif // COS_CONDITION_VARIABLE_H
