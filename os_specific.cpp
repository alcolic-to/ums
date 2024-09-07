#include "os_specific.h"

#include <cassert>
#include <cstdint>
#include <exception>
#include <iostream>

#include "io_api.h"

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

#define ENDL '\n'

constexpr uint32_t MAX_CPUS = 64;

// Windows implementations.
//
#if defined(OS_WINDOWS)

// Reduce size of windows.h includes and include windows.
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Helper function for getting result from win32 API.
//
DWORD bool_to_error(bool b) // NOLINT
{
    return b ? ERROR_SUCCESS : GetLastError();
}

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

    std::cout << "Available CPUs mask: " << procCpuMask << " System CPUs mask: " << allCpusMask
              << ENDL;

    return procCpuMask;
}

// Binds current thread to to provided CPU.
//
void bind_thread(uint64_t cpu_mask)
{
    SetThreadAffinityMask(GetCurrentThread(), cpu_mask);
}

OVERLAPPED* to_ol_ptr(IO_Control& io_ctrl)
{
    return reinterpret_cast<OVERLAPPED*>(&io_ctrl.m_ol);
}

void read_file(IO_Request& io)
{
    DWORD read_res = bool_to_error(
        bool(ReadFile(io.m_file_handle, io.m_io_buffer.m_buffer, DWORD(io.m_io_buffer.m_size),
                      nullptr, to_ol_ptr(io.m_control))));

    // std::cout << "ReadFile: " << read_res << "\n";

    switch (read_res) {
    case (ERROR_SUCCESS):
        io.m_state = IO_Request::State::completed;
        break;
    case (ERROR_IO_PENDING):
        io.m_state = IO_Request::State::pending;
        break;
    default:
        io.m_state = IO_Request::State::error;
        break;
    }
}

void write_file(IO_Request& io)
{
    DWORD write_res = bool_to_error(
        bool(WriteFile(io.m_file_handle, io.m_io_buffer.m_buffer, DWORD(io.m_io_buffer.m_size),
                       nullptr, to_ol_ptr(io.m_control))));

    // std::cout << "WriteFile: " << write_res << "\n";

    switch (write_res) {
    case (ERROR_SUCCESS):
        io.m_state = IO_Request::State::completed;
        break;
    case (ERROR_IO_PENDING):
        io.m_state = IO_Request::State::pending;
        break;
    default:
        io.m_state = IO_Request::State::error;
        break;
    }
}

bool io_completed(IO_Control& io_control)
{
    return HasOverlappedIoCompleted(to_ol_ptr(io_control));
}

void update_io_state(IO_Request& io)
{
    if (io.pending() && !io_completed(io.m_control)) {
        // std::cout << "IO still pending...\n";
        return;
    }

    DWORD bytes = 0;
    DWORD ol_res = bool_to_error(
        GetOverlappedResult(io.m_file_handle, to_ol_ptr(io.m_control), &bytes, false));

    // std::cout << "GetOverlappedResult: " << ol_res << ", bytes : " << bytes << "\n";

    switch (ol_res) {
    case (ERROR_SUCCESS):
        io.m_state = IO_Request::State::completed;
        break;
    case (ERROR_IO_PENDING):
        io.m_state = IO_Request::State::pending;
        break;
    default:
        io.m_state = IO_Request::State::error;
        break;
    }
}

#elif defined(OS_LINUX)

#include <cstring>
#include <sched.h>
#include <unistd.h>

uint32_t cpus_count()
{
    return std::min(MAX_CPUS, static_cast<std::uint32_t>(sysconf(_SC_NPROCESSORS_ONLN)));
}

uint64_t cpus_avail_mask()
{
    pid_t pid = getpid();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    uint64_t cpu_mask = 0;

    if (sched_getaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        std::cerr << "sched_getaffinity failed: " << std::strerror(errno) << ENDL;
        return -1;
    }

    int num_cores = CPU_COUNT(&mask);
    std::cout << "Process is allowed to run on " << num_cores << " cores." << ENDL;

    for (std::size_t i = 0; i < cpus_count(); ++i) {
        if (CPU_ISSET(i, &mask)) {
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

    for (std::size_t i = 0; i < cpus_count(); ++i)
        if (cpu_mask & (1 << i))
            CPU_SET(i, &mask);

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
        if (CPU_ISSET(i, &mask))
            std::cout << i << " ";

    std::cout << ENDL;
}

void read_file(IO_Request& io)
{
    throw std::logic_error{"Not implemented"};
}

void write_file(IO_Request& io)
{
    throw std::logic_error{"Not implemented"};
}

bool io_completed(IO_Control& io_control)
{
    throw std::logic_error{"Not implemented"};
}

void update_io_state(IO_Request& io)
{
    throw std::logic_error{"Not implemented"};
}

#elif defined(OS_MAC)
#include <sys/sysctl.h>

uint32_t cpus_count()
{
    int num_cpu;
    size_t len = sizeof(num_cpu);
    int mib[2] = {CTL_HW, HW_AVAILCPU};

    if (sysctl(mib, 2, &num_cpu, &len, nullptr, 0) == -1) {
        mib[1] = HW_NCPU;
        if (sysctl(mib, 2, &num_cpu, &len, nullptr, 0) == -1)
            return num_cpu = 1;
    }

    if (num_cpu < 1)
        num_cpu = 1;

    return std::min(static_cast<uint32_t>(num_cpu), MAX_CPUS);
}

// Getting availability mask for process on OSX is not supported
// Instead, we return all available CPUs
uint64_t cpus_avail_mask()
{
    uint64_t procCpuMask = 0; // This process is only allowed to run on these CPUs.
    for (uint32_t i = 0; i < cpus_count(); ++i)
        procCpuMask |= (1 << i);

    return procCpuMask;
}

// Binding thread to CPU is not supported on OSX
void bind_thread(uint64_t cpu_mask)
{
    return;
}

void read_file(IO_Request& io)
{
    throw std::logic_error{"Not implemented"};
}

void write_file(IO_Request& io)
{
    throw std::logic_error{"Not implemented"};
}

bool io_completed(IO_Control& io_control)
{
    throw std::logic_error{"Not implemented"};
}

void update_io_state(IO_Request& io)
{
    throw std::logic_error{"Not implemented"};
}

#else
static_assert(!"Unknown OS.");
#endif
