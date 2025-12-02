/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef UMS_UTIL_H
#define UMS_UTIL_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "types.h"

// clang-format off
#define NO_OP do {} while (0) // NOLINT
// clang-format on

/**
 * Defining all tracy macros with T prefix, to avoid checking every time if TRACY_ENABLE is
 * defined and including Tracy.hpp in every file that needs instrumentation. Just put T prefix
 * (TZoneScoped for example) and profile...
 * Please define all other macros that you wish to use and are not defined here.
 */
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#define TZoneScoped ZoneScoped
#define TTracyMessageL(x) TracyMessageL(x)
#else
#define TZoneScoped NO_OP
#define TTracyMessageL(x) NO_OP
#endif

namespace ums {

#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size;
#else
constexpr std::size_t cache_line_size = 64;
#endif

/**
 * Taken from google benchmark.
 */
#if defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !defined(__clang__)
#define ALWAYS_INLINE __forceinline
#define __func__ __FUNCTION__
#else
#define ALWAYS_INLINE
#endif

/**
 * Taken from google benchmark.
 */
template<class Tp>
inline ALWAYS_INLINE void dont_optimize(Tp&& value) // NOLINT
{
#if defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory"); // NOLINT
#else
    asm volatile("" : "+m,r"(value) : : "memory"); // NOLINT
#endif
}

/**
 * https://en.cppreference.com/w/cpp/utility/unreachable.html
 */
[[noreturn]] inline void unreachable()
{
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
    __assume(false);
#else // GCC, Clang
    __builtin_unreachable();
#endif
}

using namespace std::chrono;
using namespace std::chrono_literals;
using Clock = steady_clock;
using Time_point = std::chrono::time_point<Clock>;

inline Time_point now() noexcept
{
    return Clock::now();
}

/**
 * Stopwatch that uses steady_clock for time measurement.
 * You can pass time Unit for default formatting if print is specified. Default is milliseconds.
 * To measure specific part of code, just put it in a scope and create Stopwatch
 * at the beggining. For example:
 *
 * ... Code not measured ...
 *
 * {
 *     Stopwatch sw;
 *     ... Code that we want to measure ...
 *
 *     ... Measurement stops here.
 * }
 *
 * ... Code not measured ...
 */
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

/**
 * Random number generator.
 * Taken from stockfish.
 */
class PRNG {
public:
    explicit PRNG(u64 seed = u64(now().time_since_epoch().count())) noexcept : m_seed{seed} {}

    template<typename T>
    T rand() noexcept
    {
        return T(rand64());
    }

private:
    u64 rand64() noexcept
    {
        m_seed ^= m_seed >> 12, m_seed ^= m_seed << 25, m_seed ^= m_seed >> 27; // NOLINT
        return m_seed * 2685821657736338717LL;                                  // NOLINT
    }

    u64 m_seed;
};

/**
 * Random number generator.
 */
template<typename T = uint64_t>
T random() noexcept
{
    return PRNG{}.rand<T>();
}

/**
 * Splits string on provided delimiter.
 * In first pass we determine how many entries would resulting vector have, and in second, we
 * emplace results.
 */
template<class Delim>
inline std::vector<std::string> string_split(const std::string& str, const Delim& delim)
{
    if (str.empty())
        return {""};

    usize delim_len = 0;
    using DelimType = std::remove_cv_t<std::remove_reference_t<Delim>>;
    if constexpr (std::is_same_v<DelimType, char>)
        delim_len = 1;
    else if constexpr (std::is_same_v<DelimType, char*> || std::is_array_v<DelimType>)
        delim_len = std::strlen(delim);
    else if constexpr (std::is_same_v<DelimType, std::string> ||
                       std::is_same_v<DelimType, std::string_view>)
        delim_len = delim.size();
    else
        static_assert(false, "Invalid delimiter type.");

    if (delim_len == 0)
        return {str};

    usize token_count = 1;
    for (usize pos = 0; (pos = str.find(delim, pos)) != std::string::npos; pos += delim_len)
        ++token_count;

    std::vector<std::string> tokens;
    tokens.reserve(token_count);

    usize start = 0;
    usize end = str.find(delim);

    while (end != std::string::npos) {
        tokens.emplace_back(str, start, end - start);
        start = end + delim_len;
        end = str.find(delim, start);
    }

    tokens.emplace_back(str, start);
    return tokens;
}

/**
 * Trims all left spaces from string.
 */
inline void trim_left(std::string& s)
{
    s.erase(s.begin(),
            std::ranges::find_if_not(s.begin(), s.end(), [](u8 ch) { return std::isspace(ch); }));
}

/**
 * Trims all right spaces from string.
 */
inline void trim_right(std::string& s)
{
    s.erase(std::ranges::find_if_not(s.rbegin(), s.rend(), [](u8 ch) { return std::isspace(ch); })
                .base(),
            s.end());
}

/**
 * Reads file from provided path to string.
 */
inline std::string file_to_string(const std::string& path)
{
    std::ifstream f{path, std::ios_base::binary};
    return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

/**
 * Reads file from provided path to vector of chars.
 */
inline std::vector<char> file_to_vector(const std::string& path)
{
    std::ifstream f{path, std::ios_base::binary};
    return std::vector<char>{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace ums

#endif // UMS_UTIL_H