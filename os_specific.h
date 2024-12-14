#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

#ifdef __linux__
#include <liburing.h>
#endif

class IO_Request;
struct IO_Control;

namespace os {

// CPU and thread related functions.
//
uint32_t cpus_count() noexcept; // maybe just std::thread::hardware_concurrency?
uint64_t cpus_avail_mask() noexcept;
void bind_thread(uint64_t cpu_mask) noexcept;
void print_thread_affinity() noexcept;

// I/O functions.
//
void read_file(IO_Request& io) noexcept;
void write_file(IO_Request& io) noexcept;
void update_io_state(IO_Request& io) noexcept;
void* alloc_io_handle(uint64_t offset);
void free_io_handle(IO_Request& io) noexcept;

// File functions.
//
void* open_file(const char* file_path, int flags, int mode);
void close_file(void* file_handle);

#ifdef __linux__

struct IO_handle {
    io_uring* m_uring;
    uint64_t m_id;
};

class IO_uring_raii
{
public:
    IO_uring_raii() noexcept
    {
        io_uring_queue_init(1, &m_ring, 0 /* flags */);
    }

    ~IO_uring_raii() noexcept
    {
        io_uring_queue_exit(&m_ring);
    }

    io_uring m_ring{};
};

extern thread_local IO_uring_raii tls_uring; // NOLINT

#endif // __linux__

} // namespace os

#endif // OS_SPECIFIC_H
