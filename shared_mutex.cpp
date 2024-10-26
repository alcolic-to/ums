#include "shared_mutex.h"

#include <mutex>

#include "mutex.h"

// Shared mutex implementation from Howard Hinnant:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2406.html

Shared_mutex::~Shared_mutex() noexcept
{
    const std::lock_guard<Mutex> lock{m_mutex};
}

// TODO: Check whether exception should be thrown in case of a deadlock.
// STL implementation does nothing in that case.
//
void Shared_mutex::lock()
{
    std::unique_lock<Mutex> lk{m_mutex};
    m_gate1.wait(lk, [&] { return !m_state.has_write(); });
    m_state.set_write();

    m_gate2.wait(lk, [&] { return !m_state.has_readers(); });
}

bool Shared_mutex::try_lock() noexcept
{
    const std::unique_lock<Mutex> lk{m_mutex};
    if (!m_state.has_readers() && !m_state.has_write()) {
        m_state.set_write();
        return true;
    }
    else
        return false;
}

void Shared_mutex::unlock() noexcept
{
    {
        const std::lock_guard<Mutex> lk{m_mutex};
        m_state.unset_write();
    }

    m_gate1.notify_all();
}

void Shared_mutex::lock_shared()
{
    std::unique_lock<Mutex> lk{m_mutex};
    m_gate1.wait(lk, [&] { return !m_state.has_write() && !m_state.max_readers(); });
    m_state.inc_readers();
}

bool Shared_mutex::try_lock_shared() noexcept
{
    const std::unique_lock<Mutex> lk{m_mutex};
    if (!m_state.has_write() && !m_state.max_readers()) {
        m_state.inc_readers();
        return true;
    }
    else
        return false;
}

void Shared_mutex::unlock_shared() noexcept
{
    bool max_readers = false;
    bool has_readers = false;
    bool has_write = false;

    {
        const std::lock_guard<Mutex> lk{m_mutex};
        max_readers = m_state.max_readers();
        m_state.dec_readers();
        has_readers = m_state.has_readers();
        has_write = m_state.has_write();
    }

    if (has_write) {
        if (!has_readers)
            m_gate2.notify_one();
    }
    else {
        if (max_readers)
            m_gate1.notify_one();
    }
}
