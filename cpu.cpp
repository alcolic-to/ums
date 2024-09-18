#include "cpu.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>

#include "config.h"
#include "os_specific.h"

CPU::CPU(uint64_t cpu_id, Cpu_Mask cpu_mask) noexcept
    : m_id{cpu_id}
    , m_mask{cpu_mask}
    , m_scheduler{*this}
{
}

// Creates new CPU for each bit available in available CPUs mask.
//
// clang-format off
CPUs::CPUs() noexcept try
    : m_system_cpus_count{cpus_count()}
    , m_avail_cpus_mask{Cpu_Mask{cpus_avail_mask()} & CFG_allowed_cpus}
{
    for (uint64_t cpu_id = 0; cpu_id < m_avail_cpus_mask.size(); ++cpu_id)
        if (m_avail_cpus_mask.test(cpu_id))
            m_cpus.emplace_back(std::make_unique<CPU>(cpu_id, Cpu_Mask{}.set(cpu_id)));
}
catch (...) {
    std::terminate();
}

// clang-format on

Scheduler& CPUs::min_load_scheduler() const
{
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    const auto cmp = [](const auto& left, const auto& right) {
        return left->m_scheduler.load() < right->m_scheduler.load();
    };

    return (*std::min_element(m_cpus.begin(), m_cpus.end(), cmp))->m_scheduler;
}
