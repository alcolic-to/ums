#include "mutex.h"

#include <system_error>

#include "spinlock.h"
#include "worker.h"

void Mutex::lock()
{
    const lock_error err = m_lock.lock<lock_type::try_lock>();

    if (err == lock_error::success)
        return;

    if (err == lock_error::deadlock)
        throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));

    while (m_lock.lock<lock_type::single_try_lock>() != lock_error::success)
        tls_worker->yield();
};

// TODO: Check whether we should just single try lock.
//
bool Mutex::try_lock() noexcept
{
    return m_lock.lock<lock_type::try_lock>() == lock_error::success;
};

// Since standard requires that unlock does not throw, we can not throw operation_not_permitted
// here. In case that we want to support error checking here, we can return lock_error from spinlock
// and thow operation_not_permitted if error is deadlock.
//
// std::thread::id expected_tid{std::this_thread::get_id()};
// lock_error err = CAS(m_tid, expected_tid, empty_tid);
// ...
//
void Mutex::unlock() noexcept
{
    m_lock.unlock();
};
