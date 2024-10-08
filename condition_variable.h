#pragma once

#ifndef COS_CONDITION_VARIABLE_H
#define COS_CONDITION_VARIABLE_H

#include <mutex>
#include <vector>

#include "mutex.h"
#include "worker.h"

class Condition_variable {
public:
    Condition_variable() noexcept = default;
    ~Condition_variable() noexcept = default;

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

    void notify_one() noexcept;
    void notify_all() noexcept;

private:
    void add_waiter();
    void remove_waiter() noexcept;
    void notify_waiter() noexcept;

    Spinlock m_waiters_lock;
    std::vector<Worker*> m_waiters;
};

#endif // COS_CONDITION_VARIABLE_H
