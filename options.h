#pragma once

#ifndef OPTIONS_H
#define OPTIONS_H

#include <algorithm>
#include <cstdint>

#include "config.h"

namespace ums {

class Options {
public:
    class Option {
    public:
        Option(uint64_t value, uint64_t min, uint64_t max) noexcept
            : m_value{std::clamp(value, min, max)}
        {
        }

        operator uint64_t() const noexcept { return m_value; } // NOLINT

    private:
        uint64_t m_value;
    };

    struct Schedulers_count : Option {
        explicit Schedulers_count(uint64_t value = CFG_default_schedulers_count) noexcept
            : Option{value, CFG_min_schedulers_count, CFG_max_schedulers_count}
        {
        }
    };

    struct Workers_per_scheduler : Option {
        explicit Workers_per_scheduler(uint64_t value = CFG_default_workers_per_scheduler) noexcept
            : Option{value, CFG_min_workers_per_scheduler, CFG_max_workers_per_scheduler}
        {
        }
    };

    explicit Options(Schedulers_count schedulers_count = Schedulers_count{},
                     Workers_per_scheduler workers_per_scheduler = Workers_per_scheduler{}) noexcept
        : m_schedulers_count{schedulers_count}
        , m_workers_per_scheduler{workers_per_scheduler}
    {
    }

    Schedulers_count schedulers_count() { return m_schedulers_count; }

    Workers_per_scheduler workers_per_scheduler() { return m_workers_per_scheduler; }

private:
    // Maximum number of schedulers that will be created.
    //
    Schedulers_count m_schedulers_count;

    // Number of workers per scheduler.
    //
    Workers_per_scheduler m_workers_per_scheduler;
};

} // namespace ums

#endif // OPTIONS_H
