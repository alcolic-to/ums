#include <cstdint>
#include <stdexcept>

#include "cos.h"

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_OTHER
#endif

// Windows implementations.
//
#if defined OS_WINDOWS

// Reduce size of windows.h includes and include windows.
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Returns number of CPUs in the system.
//
uint32_t CpusCount()
{
	return GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
}

// Returns availability mask for this process.
// Example: returns 15 (0b0000000000001111) -> 4 CPUs are available (0, 1, 2, 3).
//
uint64_t CpusAvailMask()
{
	uint64_t procCpuMask = 0; // This process is only allowed to run on these CPUs.
	uint64_t allCpusMask = 0; // These are all available CPUs in the system.

	GetProcessAffinityMask(GetCurrentProcess(), &procCpuMask, &allCpusMask);

	std::cout << "Available CPUs mask: " << procCpuMask << " System CPUs mask: " << allCpusMask << '\n';

	return procCpuMask;
}

// Binds current thread to this CPU.
//
void CPU::BindThread()
{
	SetThreadAffinityMask(GetCurrentThread(), m_mask);
}

#elif define OS_LINUX

#include <unistd.h>

uint32_t CpusCount()
{
	return sysconf(_SC_NPROCESSORS_ONLN);;
}

uint64_t CpusAvailMask()
{
	throw std::logic_error("Not implemented.");
	return -1;
}

// Binds current thread to this CPU.
//
void CPU::BindThread()
{
	throw std::logic_error("Not implemented.");
}

#else
#endif