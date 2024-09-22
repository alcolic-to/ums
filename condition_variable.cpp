#include "condition_variable.h"

#include <mutex>
#include <vector>

#include "mutex.h"
#include "worker.h"

void Condition_variable::wait(std::unique_lock<Mutex>& lock)
{
    add_waiter();

    {
        tls_worker->clear_cond();
        const scoped_unlock<std::unique_lock<Mutex>> l{lock};
        tls_worker->wait_condition();
    }

    remove_waiter();
}

void Condition_variable::notify_one() noexcept
{
    notify_waiter();
}

void Condition_variable::notify_all() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    for (auto&& waiter : m_waiters)
        waiter->set_cond();
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
void Condition_variable::remove_waiter() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    std::erase(m_waiters, tls_worker);
}

void Condition_variable::notify_waiter() noexcept
{
    const std::scoped_lock<Spinlock> lock{m_waiters_lock};
    if (!m_waiters.empty())
        m_waiters.front()->set_cond();
}
