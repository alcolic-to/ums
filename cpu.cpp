#include "cpu.h"

#include "config.h"
#include "os_specific.h"

CPU::CPU(uint64_t cpu_id, uint64_t cpu_mask) : m_id{cpu_id}, m_mask{cpu_mask}, m_scheduler{*this} {}

// Creates new CPU for each bit available in available CPUs mask.
//
CPUs::CPUs() : m_system_cpus_count{cpus_count()}, m_avail_cpus_mask{cpus_avail_mask()}
{
    m_avail_cpus_mask &= CFG_allowed_cpus;

    for (uint64_t cpu_id = 0, cpus_mask = m_avail_cpus_mask; cpus_mask != 0;
         ++cpu_id, cpus_mask >>= 1)
        if (cpus_mask & 1)
            m_cpus.emplace_back(std::make_unique<CPU>(cpu_id, 1 << cpu_id));
}

Scheduler& CPUs::min_load_scheduler() const
{
    constexpr auto cmp = [](const std::unique_ptr<CPU>& left, const std::unique_ptr<CPU>& right) {
        return left->m_scheduler.load() < right->m_scheduler.load();
    };

    return (*std::min_element(m_cpus.begin(), m_cpus.end(), cmp))->m_scheduler;
}

// Global cpus.
//
CPUs cpus;