#pragma once

#ifndef COS_CONFIG_H
#define COS_CONFIG_H

#include <bitset>
#include <chrono>
#include <cstdint>

using namespace std::chrono_literals;

// clang-format off
constexpr uint32_t                       CFG_max_cpu_count   = 64;
constexpr std::bitset<CFG_max_cpu_count> CFG_allowed_cpus    = 0b00111111;
constexpr uint32_t                       CFG_workers_per_cpu = 4;
constexpr auto                           CFG_idle_sleep      = 20ns;
// clang-format on

#endif // COS_CONFIG_H
