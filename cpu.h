#pragma once

#ifndef COS_CPU_H
#define COS_CPU_H

#include <bitset>
#include <cstdint>
#include <memory>
#include <vector>

#include "config.h"
#include "scheduler.h"

using Cpu_Mask = std::bitset<CFG_max_cpu_count>;

class CPU final {
public:
    CPU(uint64_t cpu_id, Cpu_Mask cpu_mask) noexcept;

    uint64_t m_id;
    Cpu_Mask m_mask;

    Scheduler m_scheduler;
};

class CPUs final {
public:
    CPUs() noexcept;

    [[nodiscard]] Scheduler& min_load_scheduler() const;
    [[nodiscard]] uint32_t workers_count() const;
    [[nodiscard]] uint32_t count() const;

private:
    uint32_t m_system_cpus_count;
    Cpu_Mask m_avail_cpus_mask;
    std::vector<std::unique_ptr<CPU>> m_cpus;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern CPUs cpus;

#endif // COS_CPU_H