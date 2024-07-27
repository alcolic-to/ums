#pragma once

#ifndef COS_CPU_H
#define COS_CPU_H

#include <cstdint>
#include <vector>
#include <memory>

#include "scheduler.h"

class CPU final
{
public:
	CPU(uint64_t cpu_id, uint64_t cpu_mask);

public:
	uint64_t m_id;
	uint64_t m_mask;

	Scheduler m_scheduler;
};

class CPUs final
{
public:
	CPUs();

	Scheduler& min_load_scheduler() const;

private:
	uint32_t m_system_cpus_count;
	uint64_t m_avail_cpus_mask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

extern CPUs cpus;

#endif // COS_CPU_H