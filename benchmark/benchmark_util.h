// NOLINTBEGIN

#pragma once

#ifndef BENCHMARK_UTIL_H
#define BENCHMARK_UTIL_H

#include <atomic>
#include <cstdint>
#include <vector>

#include "util.h"

using namespace ums;

// Since we don't support dynamic caches detection yet, we will simulte some values.
//
constexpr uint64_t L1_size = 48 * 1024;        // 48 KiB
constexpr uint64_t L2_size = 2 * 1024 * 1024;  // 2  MiB
constexpr uint64_t L3_size = 24 * 1024 * 1024; // 24 MiB

// Clears caches (replaces with random data) by forcing access to heap memory and
// moving it to caches.
//
void clear_caches(size_t size = L1_size + L2_size + L3_size)
{
    std::vector<std::atomic<size_t>> v(size / sizeof(size_t));
    for (auto& b : v) [[maybe_unused]]
        auto tmp = b.load(std::memory_order_acquire);
}

// Simulates work with specified duration.
//
void hard_work(auto dur, bool memory_access = false)
{
    Stopwatch<false> s;

    if (memory_access) {
        constexpr size_t caches_size = L1_size + L2_size + L3_size;
        std::vector<std::atomic<size_t>> v(caches_size / sizeof(size_t));

        while (true) {
            for (auto& b : v) {
                [[maybe_unused]] auto tmp = b.load(std::memory_order_acquire);
                if (s.elapsed() >= dur)
                    return;
            }
        }
    }
    else {
        while (s.elapsed() < dur)
            ;
    }
}

#endif // BENCHMARK_UTIL_H

// NOLINTEND
