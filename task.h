#pragma once

#ifndef COS_TASK_H
#define COS_TASK_H

#include <deque>
#include <functional>
#include <memory>

#include "condition_variable.h"
#include "mutex.h"
#include "spinlock.h"
#include "util.h"

class Task {
public:
    enum class State : int { not_started, running, done };

    explicit Task(std::function<void()> function) noexcept;

    void wait();
    void notify() noexcept;
    void operator()();

    std::function<void()> m_func;
    State m_state{State::not_started};
    Mutex m_mtx;
    Condition_variable m_cv;
};

// Tasks queue which allows concurrent access.
// Note that all memory operations can be relaxed, because spinlock has acquire-release memory
// ordering.
//
class alignas(cache_line_size) Tasks {
public:
    void enque(std::shared_ptr<Task> task);
    std::shared_ptr<Task> deque() noexcept;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    Spinlock m_lock;
    std::deque<std::shared_ptr<Task>> m_tasks;
    std::atomic<std::size_t> m_size{0};
};

#endif // COS_TASK_H
