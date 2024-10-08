#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

class IO_Request;
struct IO_Control;

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
bool io_completed(IO_Control& io_control) noexcept;
void update_io_state(IO_Request& io) noexcept;

#endif // OS_SPECIFIC_H
