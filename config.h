#pragma once

#ifndef COS_CONFIG_H
#define COS_CONFIG_H

#include <cstdint>
#include <chrono>

using namespace std::chrono_literals;

constexpr uint64_t CFG_allowed_cpus = 0b0000'0011;
constexpr int      CFG_workers_per_cpu = 4;
constexpr auto     CFG_idle_sleep = 20ns;

#endif // COS_CONFIG_H