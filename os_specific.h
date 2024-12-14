#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

class IO_Request;

#if defined _WIN32

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
    explicit IO_handle(uint64_t offset);
    Overlapped* get_ol_ptr() { return &m_ol; }
    Overlapped m_ol;
};

#elif defined __linux__

// Forward declaration.
struct io_uring;

struct IO_handle final {
    explicit IO_handle(uint64_t offset);
    io_uring* m_uring;
    uint64_t m_id;
};

#endif

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

// File functions.
//
void* open_file(const char* file_path, int flags, int mode);
void close_file(void* file_handle);

} // namespace os

#endif // OS_SPECIFIC_H
