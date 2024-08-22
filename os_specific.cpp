#include <cstdint>
#include <iostream>
#include <cassert>
#include <exception>

#include "os_specific.h"
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
DWORD bool_to_error(bool b)
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

    std::cout << "Available CPUs mask: " << procCpuMask << " System CPUs mask: " << allCpusMask << ENDL;

    return procCpuMask;
}

// Binds current thread to to provided CPU.
//
void bind_thread(uint64_t cpu_mask)
{
    SetThreadAffinityMask(GetCurrentThread(), cpu_mask);
}

OVERLAPPED* to_ol_ptr(void* io_handle)
{
    return reinterpret_cast<OVERLAPPED*>(io_handle);
}

void read_file(IO_Request& io)
{
    DWORD read_res = bool_to_error(ReadFile(io.m_file_handle, io.m_buffer, DWORD(io.m_nbytes), nullptr, to_ol_ptr(io.m_io_handle)));

    // std::cout << "ReadFile: " << read_res << "\n";

    switch (read_res)
    {
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
    DWORD write_res = bool_to_error(WriteFile(io.m_file_handle, io.m_buffer, DWORD(io.m_nbytes), nullptr, to_ol_ptr(io.m_io_handle)));

    // std::cout << "WriteFile: " << write_res << "\n";

    switch (write_res)
    {
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

bool io_completed(IO_Request& io)
{
    return HasOverlappedIoCompleted(to_ol_ptr(io));
}

void update_io_state(IO_Request& io)
{
    if (io.pending() && !io_completed(io))
    {
        // std::cout << "IO still pending...\n";
        return;
    }

    DWORD bytes = 0;
    DWORD ol_res = bool_to_error(GetOverlappedResult(io.m_file_handle, to_ol_ptr(io.m_io_handle), &bytes, false));

    // std::cout << "GetOverlappedResult: " << ol_res << ", bytes : " << bytes << "\n";

    switch (ol_res)
    {
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

void* alloc_io_handle(uint64_t offset)
{
    OVERLAPPED* ol = new OVERLAPPED();
    ol->Offset = offset & 0xFFFFFFFF;
    ol->OffsetHigh = (offset >> 32) & 0xFFFFFFFF;;
    return reinterpret_cast<void*>(ol); 
}

void free_io_handle(void* io_handle)
{
    delete reinterpret_cast<OVERLAPPED*>(io_handle);
}

#elif defined(OS_LINUX)

#include <unistd.h>
#include <liburing.h>
#include <sched.h>
#include <cstring>

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

    if (sched_getaffinity(pid, sizeof(cpu_set_t), &mask) == -1)
    {
        std::cerr << "sched_getaffinity failed: " << std::strerror(errno) << ENDL;
        return -1;
    }

    int num_cores = CPU_COUNT(&mask);
    std::cout << "Process is allowed to run on " << num_cores << " cores." << ENDL;

    for (std::size_t i = 0; i < cpus_count(); ++i)
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

    for (std::size_t i = 0; i < cpus_count(); ++i)
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

uint64_t IO_Uring::get_io_request_id(IO_Request& io)
{
    return *reinterpret_cast<uint64_t*>(io.m_io_handle);
}

void IO_Uring::submit(IO_Request& io)
{
    io_uring& ring = tls_uring.m_ring;
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if (!sqe)
        assert(!"io_uring_get_sqe returned nullptr!");

    iovec io_vec;
    io_vec.iov_base = io.m_buffer;
    io_vec.iov_len = io.m_nbytes;
    int fd = *reinterpret_cast<int*>(io.m_file_handle);
    int offset = io.m_offset;

    if (io.m_type == IO_Request::Type::write)
        io_uring_prep_writev(sqe, fd, &io_vec, 1, offset);
    else
        io_uring_prep_readv(sqe, fd, &io_vec, 1, offset);
    io_uring_sqe_set_data64(sqe, get_io_request_id(io));
    
    int ret = io_uring_submit(&ring);
    if (ret != 1)
        assert(!"io_uring_submit should return 1!");

    io.set_pending();
}


void IO_Uring::update_io_state(IO_Request& io)
{
    if (io.m_state != IO_Request::State::pending)
        return;

    io_uring& ring = tls_uring.m_ring;
    io_uring_cqe* cqe;
    int ret = io_uring_peek_cqe(&ring, &cqe);
    if (ret == 0)
    {
        // Not sure what we do if ret is 0 but cqe is nullptr
        // Fail request?
        assert(cqe != nullptr && "cqe must not be nullptr");
        
        if (cqe->res != io.m_nbytes)
            io.set_error();
        else
            io.set_completed();
        uint64_t req_id = io_uring_cqe_get_data64(cqe);
        
        assert(req_id == get_io_request_id(io) && "Missmatch between req_id and cqe data64!");
        
        io_uring_cqe_seen(&ring, cqe);
    }
    // TODO milant: HANDLE POSSIBLE RETURN VALUES
}

void update_io_state(IO_Request& io)
{
    tls_uring.update_io_state(io);
}

void read_file(IO_Request& io)
{ 
    tls_uring.submit(io);
}

void write_file(IO_Request& io)
{
    tls_uring.submit(io);
}

void* alloc_io_handle(uint64_t offset)
{
    (void) offset;
    static std::atomic<uint64_t> io_cnt { 0 };
    uint64_t* io_id = new uint64_t(io_cnt.fetch_add(1));
    return reinterpret_cast<void*>(io_id);
}

void free_io_handle(void* io_handle)
{
    uint64_t* io_id = reinterpret_cast<uint64_t*>(io_handle);
    delete io_id;
}

thread_local IO_Uring tls_uring;

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

void read_file(IO_Request& io) { throw std::logic_error{ "Not implemented" }; }
void write_file(IO_Request& io) { throw std::logic_error{ "Not implemented" }; }
void update_io_state(IO_Request& io) { throw std::logic_error{ "Not implemented" }; }
void* alloc_io_handle(uint64_t offset) { throw std::logic_error{ "Not implemented" }; }
void free_io_handle(void* io_handle) { throw std::logic_error{ "Not implemented" }; }

#else
static_assert(!"Unknown OS.");
#endif
