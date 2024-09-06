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
    Task(const std::function<void()> function);

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
    Task_manager(const CPUs& cpus);

    template<bool async>
    void execute_task(const std::function<void()> func);

    const CPUs& m_cpus;
};

extern Task_manager task_manager;

#endif // COS_TASK_MANAGER_H