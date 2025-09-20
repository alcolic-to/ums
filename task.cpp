
#include "task.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "mutex.h"
#include "spinlock.h"

namespace ums {

Task::Task(std::function<void()> function) noexcept : m_func{std::move(function)} {}

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

void Tasks::enque(std::shared_ptr<Task> task)
{
    const std::scoped_lock<Spinlock> l{m_lock};

    m_tasks.push_back(std::move(task));
    m_size.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<Task> Tasks::deque() noexcept
{
    const std::scoped_lock<Spinlock> l{m_lock};

    if (m_tasks.empty())
        return nullptr;

    std::shared_ptr<Task> t{std::move(m_tasks.front())};
    m_tasks.pop_front();

    m_size.fetch_sub(1, std::memory_order_relaxed);
    return t;
}

[[nodiscard]] size_t Tasks::size() const noexcept
{
    return m_size.load(std::memory_order_relaxed);
}

[[nodiscard]] bool Tasks::empty() const noexcept
{
    return size() == 0;
}

} // namespace ums
