#pragma once

#ifndef COS_TASK_MANAGER_H
#define COS_TASK_MANAGER_H

#include <condition_variable>
#include <functional>
#include <mutex>

class CPUs;

class Task {
public:
    enum class State : int { not_started, running, done };

    Task();
    explicit Task(const std::function<void()>& function);

    void wait();
    void notify();
    void operator()();

    std::function<void()> m_func;
    State m_state;
    std::mutex m_mtx;
    std::condition_variable m_cv;
};

class Task_manager final {
public:
    explicit Task_manager(const CPUs& cpus) noexcept;
    ~Task_manager() = default;

    Task_manager(const Task_manager& rhs) = delete;
    Task_manager& operator=(const Task_manager& rhs) = delete;

    Task_manager(Task_manager&& rhs) noexcept = delete;
    Task_manager& operator=(Task_manager&& rhs) = delete;

    template<bool async>
    void execute_task(const std::function<void()>& func);

    const CPUs& m_cpus;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Task_manager task_manager;

#endif // COS_TASK_MANAGER_H