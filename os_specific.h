#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include "types.h"

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_UNKNOWN
#endif

namespace ums {

class IO_Request;

namespace os {

#if defined OS_WINDOWS

struct Overlapped final {
    unsigned long long m_internal, m_internal_high;

    union {
        struct {
            unsigned long m_offset, m_offset_high;
        };

        void* m_ptr;
    };

    void* m_event;
};

struct IO_handle final {
    explicit IO_handle(u64 offset);

    Overlapped* get_ol_ptr() { return &m_ol; }

    Overlapped m_ol;
};

#elif defined OS_LINUX

// Forward declaration.
struct io_uring;

struct IO_handle final {
    explicit IO_handle(uint64_t offset);
    io_uring* m_uring;
    uint64_t m_id;
};

#endif

// CPU and thread related functions.
//
u32 cpus_count() noexcept; // maybe just std::thread::hardware_concurrency?
u64 cpus_avail_mask() noexcept;
void bind_thread(u64 cpu_mask) noexcept;
void print_thread_affinity() noexcept;

// I/O functions.
//
void read_file(IO_Request& io) noexcept;
void write_file(IO_Request& io) noexcept;
void update_io_state(IO_Request& io) noexcept;

// File functions.
//
void* open_file(const char* file_path, u64 flags, u64 mode);
void close_file(void* file_handle);

} // namespace os

} // namespace ums

#endif // OS_SPECIFIC_H
