#pragma once

#ifndef COS_UTIL_H
#define COS_UTIL_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <type_traits>
#include <utility>

#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size;
#else
constexpr std::size_t cache_line_size = 64;
#endif

#define stringify2(x) #x
#define stringify(x) stringify2(x)

using namespace std::chrono;
using namespace std::chrono_literals;
using Clock = steady_clock;
using Time_point = std::chrono::time_point<Clock>;

inline Time_point now() noexcept
{
    return Clock::now();
}

// Stopwatch that uses steady_clock for time measurement.
// You can pass time Unit for default formatting if print is specified. Default is milliseconds.
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
template<bool print = true, typename Unit = milliseconds>
class Stopwatch {
public:
    explicit Stopwatch(std::string name = "Stopwatch") noexcept
        : m_name{std::move(name)}
        , m_start{now()}
    {
    }

    ~Stopwatch() noexcept
    {
        if constexpr (print) {
            auto out = elapsed_units().count();
            std::cout << m_name << " elapsed time: " << out << " " << unit_name() << "\n";
        }
    }

    Stopwatch(const Stopwatch& rhs) = delete;
    Stopwatch& operator=(const Stopwatch& rhs) = delete;
    Stopwatch(Stopwatch&& rhs) noexcept = delete;
    Stopwatch& operator=(Stopwatch&& rhs) = delete;

    void restart() noexcept { m_start = now(); }

    [[nodiscard]] auto elapsed() const noexcept { return now() - m_start; }

    [[nodiscard]] auto elapsed_units() const noexcept { return duration_cast<Unit>(elapsed()); }

    [[nodiscard]] std::string unit_name() const noexcept
    {
        // clang-format off
        if      constexpr (std::is_same_v<Unit, hours>)        return "hour(s)";
        else if constexpr (std::is_same_v<Unit, minutes>)      return "minute(s)";
        else if constexpr (std::is_same_v<Unit, seconds>)      return "second(s)";
        else if constexpr (std::is_same_v<Unit, milliseconds>) return "millisecond(s)";
        else if constexpr (std::is_same_v<Unit, microseconds>) return "microsecond(s)";
        else if constexpr (std::is_same_v<Unit, nanoseconds>)  return "nanosecond(s)";
        else                                                   return "unknown unit";
        // clang-format on
    }

private:
    std::string m_name;
    Clock::time_point m_start;
};

// Random number generator.
//
template<typename T = uint64_t>
T random() noexcept;

std::string file_to_string(const std::string& path);
std::vector<char> file_to_vector(const std::string& path);

#endif // COS_UTIL_H
