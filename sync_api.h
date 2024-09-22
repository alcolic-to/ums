#pragma once

#ifndef COS_SYNC_API_H
#define COS_SYNC_API_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

#include "util.h"
#include "worker.h"

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

#endif // COS_SYNC_API_H
