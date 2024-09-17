#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#include "util.h"

class ConditionalEvent final {
public:
    ConditionalEvent();
    virtual void wait();
    virtual void signal();
    virtual bool check() const;

private:
    std::atomic<bool> m_cond;
};

class TimedEvent final {
public:
    explicit TimedEvent(milliseconds time_to_sleep);
    virtual void wait();
    virtual void signal();
    [[nodiscard]] virtual bool check() const;

private:
    Clock::time_point m_start_time;
    milliseconds m_time_to_sleep;
};

void cos_sleep(milliseconds time_to_sleep);
