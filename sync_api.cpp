#include "sync_api.h"
#include "utils.h"
#include "worker.h"

void Event::wait() { tls_worker->wait_event(this); }

ConditionalEvent::ConditionalEvent() : m_cond(false) {}

void ConditionalEvent::wait() { Base::wait(); }

void ConditionalEvent::signal() { m_cond = true; }

bool ConditionalEvent::check() const { return m_cond; }

TimedEvent::TimedEvent(std::uint64_t time_to_sleep_in_ms)
    : m_time_to_sleep_in_ms(time_to_sleep_in_ms) {}

void TimedEvent::wait()
{
    m_start_time = get_time_in_ms();
    Base::wait();
}

void TimedEvent::signal() {}

bool TimedEvent::check() const
{
    std::uint64_t diff = get_time_in_ms() - m_start_time;
    return diff >= m_time_to_sleep_in_ms;
}


// Sleep for specified amount of time.
// * This method will yield
// * Once the time is up, it will wake up the worker.
void cos_sleep(std::uint32_t miliseconds)
{
    TimedEvent timed_event(miliseconds);
    timed_event.wait();
}
