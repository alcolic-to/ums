#pragma once

#ifndef COS_TASK_MANAGER_H
#define COS_TASK_MANAGER_H

#include <condition_variable>
#include <functional>
#include <mutex>

class Schedulers;

class Task {
public:
    enum class State : int { not_started, running, done };

    Task() noexcept;
    explicit Task(const std::function<void()>& function) noexcept;

    void wait();
    void notify() noexcept;
    void operator()();

    std::function<void()> m_func;
    State m_state;
    std::mutex m_mtx;
    std::condition_variable m_cv;
};

class Task_manager final {
public:
    explicit Task_manager(const Schedulers& schedulers) noexcept;
    ~Task_manager() noexcept = default;

    Task_manager(const Task_manager& rhs) = delete;
    Task_manager& operator=(const Task_manager& rhs) = delete;

    Task_manager(Task_manager&& rhs) noexcept = delete;
    Task_manager& operator=(Task_manager&& rhs) = delete;

    void enque_task(const std::shared_ptr<Task>& task);

    template<bool async = true>
    void execute_task(const std::function<void()>& func);

    template<bool async, typename Fn, typename... Fns>
    void execute_tasks_helper(auto& tasks, Fn fn, Fns... fns)
    {
        std::shared_ptr<Task> task = std::make_shared<Task>(fn);
        enque_task(task);
        tasks.push_back(std::move(task));

        this->execute_tasks_helper<async>(tasks, fns...);
    }

    template<bool async>
    constexpr void execute_tasks_helper(auto& tasks)
    {
    }

    template<bool async = true, typename... Fns>
    constexpr void execute_tasks(Fns... fns)
    {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(sizeof...(Fns));

        this->execute_tasks_helper<async>(tasks, fns...);

        if constexpr (!async)
            for (auto&& task : tasks)
                task->wait();
    }

    const Schedulers& m_schedulers;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Task_manager task_manager;

#endif // COS_TASK_MANAGER_H