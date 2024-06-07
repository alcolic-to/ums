#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstdint>

uint32_t CpusCount(); // maybe just std::thread::hardware_concurrency?
uint64_t CpusAvailMask();

#endif // OS_SPECIFIC_H
