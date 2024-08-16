#pragma once

#ifndef COS_UTIL_H
#define COS_UTIL_H

#include <chrono>

// Get time in milliseconds since epoch.
inline std::uint64_t get_time_in_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

#endif // COS_UTIL_H
