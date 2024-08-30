#include "sync_api.h"
#include "util.h"
#include "worker.h"

ConditionalEvent::ConditionalEvent() : m_cond(false) {}

void ConditionalEvent::wait() { tls_worker->wait_event(this); }

void ConditionalEvent::signal() { m_cond = true; }

bool ConditionalEvent::check() const { return m_cond.load(); }

TimedEvent::TimedEvent(milliseconds time_to_sleep)
    : m_time_to_sleep(time_to_sleep) {}

void TimedEvent::wait()
{
    m_start_time = now();
    tls_worker->wait_sleep(this);
}

void TimedEvent::signal() {}

bool TimedEvent::check() const
{
    auto diff = now() - m_start_time;
    return diff >= m_time_to_sleep;
}

// Sleep for specified amount of time.
// * This method will yield
// * Once the time is up, it will wake up the worker.
void cos_sleep(milliseconds time_to_sleep)
{
    TimedEvent timed_event(time_to_sleep);
    timed_event.wait();
}
