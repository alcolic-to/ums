#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

uint32_t cpus_count(); // maybe just std::thread::hardware_concurrency?
uint64_t cpus_avail_mask();
void bind_thread(uint64_t cpu_mask);

#endif // OS_SPECIFIC_H
