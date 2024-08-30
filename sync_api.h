#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>

#include "util.h"

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
        TimedEvent(milliseconds time_to_sleep);
        virtual void wait();
        virtual void signal(); 
        virtual bool check() const;
    private:
        Clock::time_point m_start_time;
        milliseconds m_time_to_sleep;
};

void cos_sleep(milliseconds time_to_sleep);
