#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

class IO_Request;
struct IO_Control;

// CPU and thread related functions.
//
uint32_t cpus_count(); // maybe just std::thread::hardware_concurrency?
uint64_t cpus_avail_mask();
void bind_thread(uint64_t cpu_mask);
void print_thread_affinity();

// I/O functions.
//
void issue_io_with_handle(IO_Request& io);
void issue_io_with_stream(IO_Request& io);
bool io_completed(IO_Control& io_control);
void update_io_state(IO_Request& io);

#endif // OS_SPECIFIC_H
