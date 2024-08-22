#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

class IO_Request;
struct IO_Control;

#ifdef __linux__

#include <liburing.h>

class CosUring
{
public:
    CosUring()
    {
        io_uring_queue_init(1, &m_ring, 0 /* flags */);
    }

    ~CosUring()
    {
        io_uring_queue_exit(&m_ring);
    }

    uint64_t get_io_request_id(IO_Request& io);

    void submit(IO_Request& io);

    void update_io_state(IO_Request& io);

    io_uring m_ring;
};

extern thread_local CosUring tls_uring;

#elif WIN32_

struct Overlapped final
{
    unsigned long long m_internal, m_internal_high;
    union
    {
        struct { unsigned long m_offset, m_offset_high; };
        void* m_ptr;
    };

    void* m_event;
};

#endif

// CPU and thread related functions.
//
uint32_t cpus_count(); // maybe just std::thread::hardware_concurrency?
uint64_t cpus_avail_mask();
void bind_thread(uint64_t cpu_mask);
void print_thread_affinity();

// I/O functions.
//
void read_file(IO_Request& io);
void write_file(IO_Request& io);
bool io_completed(IO_Control& io_control);
void update_io_state(IO_Request& io);
void* init_io_handle(uint64_t offset);
void free_io_handle(void* io_handle);

#endif // OS_SPECIFIC_H
