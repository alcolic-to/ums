#include "task_manager.h"

#include <functional>
#include <memory>
#include <mutex>

#include "mutex.h"
#include "scheduler.h"

Task::Task() noexcept : m_state{State::not_started} {}

Task::Task(const std::function<void()>& function) noexcept
    : m_func{function}
    , m_state{State::not_started}
{
}

void Task::wait()
{
    std::unique_lock<Mutex> lock{m_mtx};
    m_cv.wait(lock, [&] { return m_state == Task::State::done; });
}

void Task::notify() noexcept
{
    const std::unique_lock<Mutex> lock{m_mtx};
    m_state = State::done;
    m_cv.notify_one();
}

void Task::operator()()
{
    m_func();
}

Task_manager::Task_manager(const Schedulers& schedulers) noexcept : m_schedulers{schedulers} {}

void Task_manager::enque_task(const std::shared_ptr<Task>& task)
{
    Scheduler& best_scheduler = m_schedulers.min_load_scheduler();
    best_scheduler.enqueue_task(task);
}

template<bool async>
void Task_manager::execute_task(const std::function<void()>& func)
{
    const std::shared_ptr<Task> task = std::make_shared<Task>(func);

    enque_task(task);

    if constexpr (!async)
        task->wait();
}

template void Task_manager::execute_task<true>(const std::function<void()>& func);
template void Task_manager::execute_task<false>(const std::function<void()>& func);
