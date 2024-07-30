#include "os_specific.h"
#include <cstdint>
#include <iostream>

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_OTHER
#endif

#define ENDL '\n'

// Windows implementations.
//
#if defined(OS_WINDOWS)

// Reduce size of windows.h includes and include windows.
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Returns number of CPUs in the system.
//
uint32_t cpus_count()
{
	return GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
}

// Returns availability mask for this process.
// Example: returns 15 (0b0000000000001111) -> 4 CPUs are available (0, 1, 2, 3).
//
uint64_t cpus_avail_mask()
{
	uint64_t procCpuMask = 0; // This process is only allowed to run on these CPUs.
	uint64_t allCpusMask = 0; // These are all available CPUs in the system.

	GetProcessAffinityMask(GetCurrentProcess(), &procCpuMask, &allCpusMask);

	std::cout << "Available CPUs mask: " << procCpuMask << " System CPUs mask: " << allCpusMask << ENDL;

	return procCpuMask;
}

// Binds current thread to to provided CPU.
//
void bind_thread(uint64_t cpu_mask)
{
	SetThreadAffinityMask(GetCurrentThread(), cpu_mask);
}

#elif defined(OS_LINUX)

#include <unistd.h>
#include <sched.h>
#include <cstring>

uint32_t cpus_count()
{
	return sysconf(_SC_NPROCESSORS_ONLN);;
}

uint64_t cpus_avail_mask()
{
	pid_t pid = getpid();
	cpu_set_t mask;
	CPU_ZERO(&mask);
	uint64_t cpu_mask = 0;

	if (sched_getaffinity(pid, sizeof(cpu_set_t), &mask) == -1)
	{
		std::cerr << "sched_getaffinity failed: " << std::strerror(errno) << ENDL;
		return -1;
	}

	int num_cores = CPU_COUNT(&mask);
	std::cout << "Process is allowed to run on " << num_cores << " cores." << ENDL;

	for (std::size_t i = 0; i < CPU_SETSIZE; ++i)
	{
		if (CPU_ISSET(i, &mask))
		{
			std::cout << "CPU " << i << " is available." << ENDL;
			cpu_mask |= (1 << i);
		}
	}

	return cpu_mask;
}

// Binds current thread to provided CPU.
//
void bind_thread(uint64_t cpu_mask)
{
	cpu_set_t mask; 
	CPU_ZERO(&mask);

	for (std::size_t i = 0; i < sizeof(cpu_mask); ++i)
	{
		if (cpu_mask & (1 << i))
			CPU_SET(i, &mask);
	}

	pthread_t current_thread = pthread_self();
	if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &mask) == -1)
		std::cerr << "pthread_setaffinity_np failed: " << std::strerror(errno) << ENDL;

	print_thread_affinity();
}

void print_thread_affinity()
{
	cpu_set_t mask;
	CPU_ZERO(&mask);

	pthread_t current_thread = pthread_self();
	if (pthread_getaffinity_np(current_thread, sizeof(cpu_set_t), &mask) == -1)
		std::cerr << "pthread_getaffinity_np failed: " << std::strerror(errno) << ENDL;

	std::cout << "Thread affinity: ";
	for (std::size_t i = 0; i < CPU_SETSIZE; ++i)
	{
		if (CPU_ISSET(i, &mask))
			std::cout << i << " ";
	}

	std::cout << ENDL;
}
#else
static_assert(!"Unknown OS.");
#endif
