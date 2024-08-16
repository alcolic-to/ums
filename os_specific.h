#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

class IO_Request;

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

struct IO_Control final
{
public:
	IO_Control(uint64_t offset) : m_ol{}
	{
		m_ol.m_offset = offset & 0xFFFFFFFF;
		m_ol.m_offset_high = (offset >> 32) & 0xFFFFFFFF;
	}

    Overlapped m_ol;
};

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

#endif // OS_SPECIFIC_H
