#include "os_specific.h"

#include <iostream> // NOLINT

#include "io_api.h"

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_UNKNOWN
#endif

#if defined(OS_WINDOWS)
// Reduce size of windows.h includes and include windows.
//
#define WIN32_LEAN_AND_MEAN
#include <bit>
#include <format>   // NOLINT
#include <windows.h>
#undef min
#undef max
#elif defined(OS_LINUX)
#include <cstring>
#include <cassert>
#include <sched.h>
#include <unistd.h>
#endif

namespace os {

// NOLINTBEGIN(misc-include-cleaner)

// Windows implementations.
//
#if defined(OS_WINDOWS)

namespace {

// Helper function for getting result from win32 API.
//
DWORD bool_to_error(BOOL b) noexcept // NOLINT
{
    return b != 0 ? ERROR_SUCCESS : GetLastError();
}

OVERLAPPED* to_ol_ptr(IO_Control& io_ctrl) noexcept
{
    return std::bit_cast<OVERLAPPED*>(&io_ctrl.m_ol);
}

} // anonymous namespace

// Returns number of CPUs in the system.
//
uint32_t cpus_count() noexcept
{
    return GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
}

// Returns availability mask for this process.
// Example: returns 15 (0b0000000000001111) -> 4 CPUs are available (0, 1, 2, 3).
//
uint64_t cpus_avail_mask() noexcept
{
    uint64_t proc_cpu_mask = 0; // Available CPUs for this process.
    uint64_t all_cpus_mask = 0; // All available CPUs in the system.

    GetProcessAffinityMask(GetCurrentProcess(), PDWORD_PTR(&proc_cpu_mask),
                           PDWORD_PTR(&all_cpus_mask));

    // std::cout << std::format("Available CPUs mask: {:b}, System CPUs mask: {:b}\n",
    // proc_cpu_mask, all_cpus_mask);

    return proc_cpu_mask;
}

// Binds current thread to the provided CPU.
//
void bind_thread(uint64_t cpu_mask) noexcept
{
    SetThreadAffinityMask(GetCurrentThread(), cpu_mask);
}

void read_file(IO_Request& io) noexcept
{
    const DWORD read_res =
        bool_to_error(ReadFile(io.m_file_handle, io.m_io_buffer.m_buffer,
                               DWORD(io.m_io_buffer.m_size), nullptr, to_ol_ptr(io.m_control)));

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

void write_file(IO_Request& io) noexcept
{
    const DWORD write_res =
        bool_to_error(WriteFile(io.m_file_handle, io.m_io_buffer.m_buffer,
                                DWORD(io.m_io_buffer.m_size), nullptr, to_ol_ptr(io.m_control)));

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

bool io_completed(IO_Control& io_control) noexcept
{
    return HasOverlappedIoCompleted(to_ol_ptr(io_control));
}

void update_io_state(IO_Request& io) noexcept
{
    if (io.pending() && !io_completed(io.m_control)) {
        // std::cout << "IO still pending...\n";
        return;
    }

    DWORD bytes = 0;
    const DWORD ol_res =
        bool_to_error(GetOverlappedResult(io.m_file_handle, to_ol_ptr(io.m_control), &bytes, 0));

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

uint32_t cpus_count() noexcept
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

uint64_t cpus_avail_mask() noexcept
{
    const pid_t pid = getpid();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    uint64_t cpu_mask = 0;

    if (sched_getaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        std::cerr << "sched_getaffinity failed: " << std::strerror(errno) << "\n";
        return -1;
    }

    const int num_cores = CPU_COUNT(&mask);
    std::cout << "Process is allowed to run on " << num_cores << " cores.\n";

    for (std::size_t i = 0; i < cpus_count(); ++i) {
        if (CPU_ISSET(i, &mask)) {
            std::cout << "CPU " << i << " is available.\n";
            cpu_mask |= (1U << i);
        }
    }

    return cpu_mask;
}

// Binds current thread to provided CPU.
//
void bind_thread(uint64_t cpu_mask) noexcept
{
    cpu_set_t mask;
    CPU_ZERO(&mask);

    for (std::size_t i = 0; i < cpus_count(); ++i)
        if ((bool)(cpu_mask & (1U << i)))
            CPU_SET(i, &mask);

    const pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &mask) == -1)
        std::cerr << "pthread_setaffinity_np failed: " << std::strerror(errno) << "\n";

    print_thread_affinity();
}

void print_thread_affinity() noexcept
{
    cpu_set_t mask;
    CPU_ZERO(&mask);

    const pthread_t current_thread = pthread_self();
    if (pthread_getaffinity_np(current_thread, sizeof(cpu_set_t), &mask) == -1)
        std::cerr << "pthread_getaffinity_np failed: " << std::strerror(errno) << "\n";

    std::cout << "Thread affinity: ";
    for (std::size_t i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &mask))
            std::cout << i << " ";

    std::cout << "\n";
}

void read_file(IO_Request& io) noexcept
{
    assert(!"Not implemented");
}

void write_file(IO_Request& io) noexcept
{

    assert(!"Not implemented");
}

bool io_completed(IO_Control& io_control) noexcept
{
    assert(!"Not implemented");
}

void update_io_state(IO_Request& io) noexcept
{
    assert(!"Not implemented");
}

// NOLINTEND(misc-include-cleaner)

#else
static_assert(!"Unknown OS.");
#endif

} // namespace os
