/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "os_specific.hpp"

#include <bit>
#include <cstdint>
#include <exception>
#include <filesystem>
// #include <format>   // NOLINT
#include <iostream> // NOLINT
#include <stdexcept>

#include "file.hpp"
#include "io.hpp"
#include "types.hpp"

// NOLINTBEGIN(misc-include-cleaner)

namespace ums {

#if defined OS_WINDOWS

// Reduce size of windows.h includes and include windows.
// NOLINTBEGIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
// NOLINTEND

constexpr i64 x_file_access = GENERIC_READ | GENERIC_WRITE;
constexpr i64 x_file_attributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS |
                                  FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING |
                                  FILE_FLAG_WRITE_THROUGH;

File_handle::File_handle(const fs::path& file_path)
    : m_handle{os::open_file(file_path.string().c_str(), x_file_access, x_file_attributes)}
{
}

// Destructor closes file handle
File_handle::~File_handle()
{
    os::close_file(m_handle);
}

namespace os {

IO_handle::IO_handle(u64 offset) : m_ol{}
{
    m_ol.m_offset = DWORD(offset & 0xFFFFFFFF);
    m_ol.m_offset_high = DWORD(offset >> 32);
}

namespace {

// Helper function for getting result from win32 API.
//
DWORD bool_to_error(BOOL b) noexcept // NOLINT
{
    return b != 0 ? ERROR_SUCCESS : GetLastError();
}

OVERLAPPED* to_windows_ol_ptr(IO_Request& io) noexcept
{
    return reinterpret_cast<OVERLAPPED*>(io.m_io_handle->get_ol_ptr());
}

} // anonymous namespace

// Returns number of CPUs in the system.
//
u32 cpus_count() noexcept
{
    return GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
}

// Returns availability mask for this process.
// Example: returns 15 (0b0000000000001111) -> 4 CPUs are available (0, 1, 2, 3).
//
u64 cpus_avail_mask() noexcept
{
    u64 proc_cpu_mask = 0; // Available CPUs for this process.
    u64 all_cpus_mask = 0; // All available CPUs in the system.

    GetProcessAffinityMask(GetCurrentProcess(), PDWORD_PTR(&proc_cpu_mask),
                           PDWORD_PTR(&all_cpus_mask));

    // std::cout << std::format("Available CPUs mask: {:b}, System CPUs mask: {:b}\n",
    // proc_cpu_mask, all_cpus_mask);

    return proc_cpu_mask;
}

// Binds current thread to the provided CPU.
//
void bind_thread(u64 cpu_mask) noexcept
{
    SetThreadAffinityMask(GetCurrentThread(), cpu_mask);
}

void read_file(IO_Request& io) noexcept
{
    const DWORD read_res =
        bool_to_error(ReadFile(io.m_file_handle, io.m_io_buffer.m_buffer,
                               DWORD(io.m_io_buffer.m_size), nullptr, to_windows_ol_ptr(io)));

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
                                DWORD(io.m_io_buffer.m_size), nullptr, to_windows_ol_ptr(io)));

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

bool io_completed(IO_Request& io) noexcept
{
    return HasOverlappedIoCompleted(to_windows_ol_ptr(io));
}

void update_io_state(IO_Request& io) noexcept
{
    if (io.pending() && !io_completed(io)) {
        // std::cout << "IO still pending...\n";
        return;
    }

    DWORD bytes = 0;
    const DWORD ol_res =
        bool_to_error(GetOverlappedResult(io.m_file_handle, to_windows_ol_ptr(io), &bytes, 0));

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

void* open_file(const char* path, u64 flags, u64 mode)
{
    HANDLE handle = CreateFile(path, flags, 0, nullptr, OPEN_ALWAYS, mode, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Failed to open file: " + std::string(path));
    return reinterpret_cast<void*>(handle);
}

void close_file(void* file_handle)
{
    HANDLE handle = reinterpret_cast<HANDLE>(file_handle);
    BOOL ret = CloseHandle(handle);
    if (ret == 0)
        throw std::runtime_error("Failed to close file handle.");
}

} // namespace os

#elif defined OS_LINUX
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

#ifdef UMS_IO_URING_ENABLED
#include <liburing.h>
#else
#pragma clang diagnostic ignored "-Wexceptions"
#pragma GCC diagnostic ignored "-Wexceptions"

struct io_uring {};

#endif // #ifdef UMS_IO_URING_ENABLED

constexpr i64 x_file_access = O_CREAT | O_RDWR | O_DIRECT;
constexpr i64 x_file_attributes = 0666;

File_handle::File_handle(const fs::path& file_path)
    : m_handle{os::open_file(file_path.string().c_str(), x_file_access, x_file_attributes)}
{
}

// Destructor closes file handle
File_handle::~File_handle()
{
    os::close_file(m_handle);
}

namespace os {

class IO_uring {
public:
    IO_uring()
    {
#ifndef UMS_IO_URING_ENABLED
        throw std::runtime_error("Failed to perform IO - uring is not built."); // NOLINT
#else
        io_uring_queue_init(1, &m_ring, 0 /* flags */);
#endif
    }

    ~IO_uring() noexcept
    {
#ifndef UMS_IO_URING_ENABLED
        throw std::runtime_error("Failed to perform IO - uring is not built."); // NOLINT
#else
        io_uring_queue_exit(&m_ring);
#endif
    }

    io_uring m_ring{};
};

thread_local IO_uring tls_uring;

IO_handle::IO_handle(u64 offset) : m_uring(&tls_uring.m_ring)
{
    (void)offset;
    static std::atomic<u64> io_cnt{0};
    m_id = io_cnt++;
}

u32 cpus_count() noexcept
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

u64 cpus_avail_mask() noexcept
{
    const pid_t pid = getpid();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    u64 cpu_mask = 0;

    if (sched_getaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        std::cerr << "sched_getaffinity failed: " << std::strerror(errno) << "\n";
        return -1;
    }

    for (std::size_t i = 0; i < cpus_count(); ++i)
        if (CPU_ISSET(i, &mask))
            cpu_mask |= (1U << i);

    return cpu_mask;
}

// Binds current thread to provided CPU.
//
void bind_thread(u64 cpu_mask) noexcept
{
    cpu_set_t mask;
    CPU_ZERO(&mask);

    for (std::size_t i = 0; i < cpus_count(); ++i)
        if ((bool)(cpu_mask & (1U << i)))
            CPU_SET(i, &mask);

    const pthread_t current_thread = pthread_self();
    if (pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &mask) == -1)
        std::cerr << "pthread_setaffinity_np failed: " << std::strerror(errno) << "\n";
}

void* open_file(const char* file_path, u64 flags, u64 mode)
{
#ifndef UMS_IO_URING_ENABLED
    throw std::runtime_error("Failed to perform IO - uring is not built."); // NOLINT
#else
    int* fd = new int(0);
    *fd = open(file_path, flags, mode);
    if (*fd == -1) {
        delete fd;
        std::cerr << "Failed to open file: " << std::strerror(errno) << "\n";
        throw std::runtime_error("Failed to open file: " + std::string(file_path));
    }

    return reinterpret_cast<void*>(fd);
#endif
}

void close_file(void* file_handle)
{
#ifndef UMS_IO_URING_ENABLED
    throw std::runtime_error("Failed to perform IO - uring is not built."); // NOLINT
#else
    int* fd = reinterpret_cast<int*>(file_handle);
    int ret = close(*fd);
    if (ret == -1) {
        std::cerr << "Failed to close file descriptor: " << std::strerror(errno) << "\n";
        throw std::runtime_error("Failed to close file descriptor.");
    }
    delete fd;
#endif
}

void uring_submit(IO_Request& io) noexcept
{
#ifndef UMS_IO_URING_ENABLED
    throw std::runtime_error("Failed to perform IO - uring is not built."); // NOLINT
#else
    const IO_handle* io_handle = io.m_io_handle.get();

    io_uring_sqe* sqe = io_uring_get_sqe(io_handle->m_uring);
    if (sqe == nullptr)
        assert(!"io_uring_get_sqe returned nullptr!");

    iovec io_vec{};
    io_vec.iov_base = io.m_io_buffer.m_buffer;
    io_vec.iov_len = io.m_io_buffer.m_size;
    const int fd = *reinterpret_cast<const int*>(io.m_file_handle);
    const u64 offset = io.m_offset;

    if (io.m_type == IO_Request::Type::write)
        io_uring_prep_writev(sqe, fd, &io_vec, 1, offset);
    else
        io_uring_prep_readv(sqe, fd, &io_vec, 1, offset);
    io_uring_sqe_set_data64(sqe, io_handle->m_id);
    const int ret = io_uring_submit(io_handle->m_uring);
    if (ret != 1)
        assert(!"io_uring_submit should return 1!");

    io.m_state = IO_Request::State::pending;
#endif
}

void uring_update(IO_Request& io) noexcept
{
#ifndef UMS_IO_URING_ENABLED
    throw std::runtime_error("Failed to perform IO - uring is not built."); // NOLINT
#else
    if (io.m_state != IO_Request::State::pending)
        return;

    const IO_handle* io_handle = io.m_io_handle.get();

    io_uring_cqe* cqe = nullptr;
    const int ret = io_uring_peek_cqe(io_handle->m_uring, &cqe);
    if (ret == 0) {
        // Not sure what we do if ret is 0 but cqe is nullptr
        // Fail request?
        assert(cqe != nullptr && "cqe must not be nullptr");

        if (cqe->res != io.m_io_buffer.m_size)
            io.m_state = IO_Request::State::error;
        else
            io.m_state = IO_Request::State::completed;

        assert(io_uring_cqe_get_data64(cqe) == io_handle->m_id &&
               "Missmatch between req_id and cqe data64!");

        io_uring_cqe_seen(io_handle->m_uring, cqe);
    }
// TODO milant: HANDLE POSSIBLE RETURN VALUES
#endif
}

void read_file(IO_Request& io) noexcept
{
    uring_submit(io);
}

void write_file(IO_Request& io) noexcept
{
    uring_submit(io);
}

void update_io_state(IO_Request& io) noexcept
{
    uring_update(io);
}

} // namespace os

// NOLINTEND(misc-include-cleaner)

#else
static_assert(!"Unknown OS.");
#endif

} // namespace ums
