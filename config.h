#pragma once

#ifndef COS_CONFIG_H
#define COS_CONFIG_H

#include <bitset>
#include <chrono>
#include <cstdint>

using namespace std::chrono_literals;

// clang-format off

// Feature switch section.

constexpr bool FS_idle_sleep_allowed = true;

// Config section.

constexpr uint32_t                       CFG_max_cpu_count        = 64;
constexpr std::bitset<CFG_max_cpu_count> CFG_allowed_cpus         = 0b01111111;
constexpr uint32_t                       CFG_workers_per_cpu      = 16;
constexpr auto                           CFG_idle_sleep_threshold = 20ms;
// clang-format on

#endif // COS_CONFIG_H
