#pragma once

#include <cstdint>
#include <atomic>

class ConditionalEvent final
{
    public:
        ConditionalEvent();
        virtual void wait();
        virtual void signal();
        virtual bool check() const;
    private:
        std::atomic<bool> m_cond;
};

class TimedEvent final
{
    public:
        TimedEvent(std::uint64_t time_to_sleep_in_ms);
        virtual void wait();
        virtual void signal(); 
        virtual bool check() const;
    private:
        std::uint64_t m_start_time;
        std::uint64_t m_time_to_sleep_in_ms;
};

void cos_sleep(std::uint32_t miliseconds);
