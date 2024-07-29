#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <cassert>

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#elif defined __APPLE__
#define OS_MAC
#else
#define OS_UNKNOWN
#endif

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

	std::cout << "Available CPUs mask: " << procCpuMask << " System CPUs mask: " << allCpusMask << '\n';

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

uint32_t cpus_count()
{
	return sysconf(_SC_NPROCESSORS_ONLN);;
}

uint64_t cpus_avail_mask()
{
	throw std::logic_error("Not implemented.");
}

// Binds current thread to provided CPU.
//
void bind_thread(uint64_t cpu_mask)
{
	throw std::logic_error("Not implemented.");
}

#elif defined(OS_MAC)
#include <sys/sysctl.h>

uint32_t cpus_count()
{
    int num_cpu;
    size_t len = sizeof(num_cpu);
    int mib[2] = { CTL_HW, HW_AVAILCPU };

    if (sysctl(mib, 2, &num_cpu, &len, nullptr, 0) == -1)
    {
        mib[1] = HW_NCPU;
        if (sysctl(mib, 2, &num_cpu, &len, nullptr, 0) == -1)
        {
            return num_cpu = 1;
        }
    }

    if (num_cpu < 1)
    {
        num_cpu = 1;
    }

    return num_cpu;
}

uint64_t cpus_avail_mask()
{
    assert(false && "Not implemented.");
    return 0;
}

void bind_thread(uint64_t cpu_mask)
{
    assert(false && "Not implemented.");
}

#else
static_assert(false && "Unknown OS.");
#endif
