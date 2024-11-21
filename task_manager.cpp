#include "task_manager.h"

#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "mutex.h"
#include "scheduler.h"

void Task::wait()
{
    std::unique_lock<Mutex> lock{m_mtx};
    m_cv.wait(lock, [&] { return m_state == Task::State::done; });
}

void Task::notify() noexcept
{
    {
        const std::unique_lock<Mutex> lock{m_mtx};
        m_state = State::done;
    }

    m_cv.notify_one();
}

void Task::operator()()
{
    m_func();
}

Task_manager::Task_manager(const Schedulers& schedulers) noexcept : m_schedulers{schedulers} {}

void Task_manager::enque_task(std::shared_ptr<Task> task)
{
    Scheduler& best_scheduler = m_schedulers.min_load_scheduler();
    best_scheduler.enqueue_task(std::move(task));
}

template<bool async>
std::shared_ptr<Task> Task_manager::execute_task(std::function<void()> func)
{
    const std::shared_ptr<Task> task{std::make_shared<Task>(std::move(func))};

    enque_task(task);

    if constexpr (!async)
        task->wait();

    return task;
}

template std::shared_ptr<Task> Task_manager::execute_task<true>(std::function<void()> func);
template std::shared_ptr<Task> Task_manager::execute_task<false>(std::function<void()> func);
