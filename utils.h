#pragma once

#include <chrono>
#include <cstdint>

// Get time in milliseconds since epoch.
inline std::uint64_t get_time_in_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

