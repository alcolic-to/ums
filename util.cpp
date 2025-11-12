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
#include "util.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

#include "types.h"

namespace ums {

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

template<typename T>
T random() noexcept
{
    return PRNG{}.rand<T>();
}

template u8 random<u8>() noexcept;
template u16 random<u16>() noexcept;
template u32 random<u32>() noexcept;
template u64 random<u64>() noexcept;

std::string file_to_string(const std::string& path)
{
    std::ifstream f{path, std::ios_base::binary};
    return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<char> file_to_vector(const std::string& path)
{
    std::ifstream f{path, std::ios_base::binary};
    return std::vector<char>{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace ums
