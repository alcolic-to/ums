#pragma once

#ifndef COS_UTIL_H
#define COS_UTIL_H

#include <chrono>
#include <iostream>

using namespace std::chrono;
using namespace std::chrono_literals;
using Clock = steady_clock;

inline auto now() { return Clock::now(); }

// RAII stopwatch that uses steady_clock for time measurement.
// You can pass Duration typename that you want to measure in. Default is milliseconds.
// To measure specific part of code, just put it in a scope and create Stopwatch
// at the beggining. For example:
//
// ... Code not measured ...
// 
// {
//     Stopwatch sw;
//     ... Code that we want to measure ...
// 
//     ... Measurement stops here.
// }
// 
// ... Code not measured ...
//
template <typename Duration = milliseconds>
class Stopwatch
{
public:
    Stopwatch(const std::string& name = "Stopwatch")
        : m_name{ name }
        , m_start{ now() }
    { }

    ~Stopwatch()
    {
        stop();
        std::cout << m_name << " elapsed time: " << elapsed().count() << " " << unit_name() << "\n";
    }

    void restart() { m_start = now(); }
    void stop() { m_end = now(); }
    auto elapsed() const { return duration_cast<Duration>(m_end-m_start); }

    static std::string unit_name()
    {
        if      constexpr (std::is_same_v<Duration, hours>)        return "hours";
        else if constexpr (std::is_same_v<Duration, minutes>)      return "minutes";
        else if constexpr (std::is_same_v<Duration, seconds>)      return "seconds";
        else if constexpr (std::is_same_v<Duration, milliseconds>) return "milliseconds";
        else if constexpr (std::is_same_v<Duration, microseconds>) return "microseconds";
        else if constexpr (std::is_same_v<Duration, nanoseconds>)  return "nanoseconds";
        else                                                       return "unknown unit";
    }

private:
    std::string m_name;
    Clock::time_point m_start;
    Clock::time_point m_end;
};

#endif // COS_UTIL_H
