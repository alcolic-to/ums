/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <algorithm>
#include <cstdint>

#include "config.hpp"
#include "types.hpp"

namespace ums {

class Options {
public:
    class Option {
    public:
        Option(u64 value, u64 min, u64 max) noexcept : m_value{std::clamp(value, min, max)} {}

        operator u64() const noexcept { return m_value; } // NOLINT

    private:
        u64 m_value;
    };

    struct Schedulers_count : Option {
        explicit Schedulers_count(u64 value = CFG_default_schedulers_count) noexcept
            : Option{value, CFG_min_schedulers_count, CFG_max_schedulers_count}
        {
        }
    };

    struct Workers_per_scheduler : Option {
        explicit Workers_per_scheduler(u64 value = CFG_default_workers_per_scheduler) noexcept
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

#endif // OPTIONS_HPP
