#pragma once

#ifndef COS_CONFIG_H
#define COS_CONFIG_H

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

#endif // COS_CONFIG_H
