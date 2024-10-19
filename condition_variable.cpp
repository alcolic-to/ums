#include "condition_variable.h"

#include <condition_variable>
#include <mutex>
#include <vector>

#include "mutex.h"
#include "spinlock.h"
#include "util.h"
#include "worker.h"

Condition_variable::~Condition_variable() noexcept
{
    notify_all();
}

void Condition_variable::wait(std::unique_lock<Mutex>& lock)
{
    wait_internal(lock, Time_point::max());
}

void Condition_variable::notify_one() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    if (!m_waiters.empty())
        m_waiters.front()->notify_waiter();
}

void Condition_variable::notify_all() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    for (auto&& waiter : m_waiters)
        waiter->notify_waiter();
}

void Condition_variable::add_waiter()
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    m_waiters.push_back(tls_worker);
}

// Since notify_all wakes up all waiters, we don't know
// which one will be first awaken, so we must call std::erase
// to remove worker, instead of pop_front.
//
void Condition_variable::remove_waiter()
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    std::erase(m_waiters, tls_worker);
}

void Condition_variable::wait_internal(std::unique_lock<Mutex>& lock, const Time_point& time_point)
{
    add_waiter();

    {
        tls_worker->set_wait_info(false, time_point);
        const scoped_unlock<std::unique_lock<Mutex>> unlock{lock};
        tls_worker->wait_cond_or_sleep();
    }

    remove_waiter();
}

std::cv_status Condition_variable::wait_until_internal(std::unique_lock<Mutex>& lock,
                                                       const Time_point& time_point)
{
    wait_internal(lock, time_point);
    return now() >= time_point ? std::cv_status::timeout : std::cv_status::no_timeout;
}
