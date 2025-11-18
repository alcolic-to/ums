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

#ifndef UMS_CONFIG_H
#define UMS_CONFIG_H

#include <chrono>
#include <cstdint>

namespace ums {

using namespace std::chrono_literals;

// clang-format off

// Feature switch section.

constexpr bool FS_thread_binding_allowed = true;
constexpr bool FS_work_stealing_allowed = true;
constexpr bool FS_idle_spinning_allowed = false;

// Config section.

// Maximum number of CPUs that we support.
//
constexpr uint32_t CFG_max_supported_cpus = 64;

// Allowed CPUs mask in the system. Schedulers will be created only on allowed CPUs.
// CPUs ordinal numbers starts from LSB (rightmost bit).
// For disallowing schedulers to be started on, for example, CPU 0, just flip last bit from 1 to 0.
//
constexpr uint64_t CFG_allowed_cpus_mask = 0b11111111'11111111'11111111'11111111'11111111'11111111'11111111'11111111;

// Number of schedulers that will be created.
// TODO: schedulers does not operate on Low Power Efficient-cores, hence those should be excluded.
//
constexpr uint64_t CFG_min_schedulers_count     = 1;
constexpr uint64_t CFG_max_schedulers_count     = CFG_max_supported_cpus;
constexpr uint64_t CFG_default_schedulers_count = 7;

// Number of workers per scheduler.
//
constexpr uint64_t CFG_min_workers_per_scheduler     = 1;
constexpr uint64_t CFG_max_workers_per_scheduler     = 256;
constexpr uint64_t CFG_default_workers_per_scheduler = 64;

// Thresholds.
//
constexpr auto CFG_idle_sleep_threshold = 20ms;
constexpr auto CFG_idle_spin_threshold  = 20ms;

// clang-format on

} // namespace ums

#endif // UMS_CONFIG_H
