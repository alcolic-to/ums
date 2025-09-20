#include "util.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace ums {

class PRNG {
public:
    explicit PRNG(uint64_t seed = uint64_t(now().time_since_epoch().count())) noexcept
        : m_seed{seed}
    {
    }

    template<typename T>
    T rand() noexcept
    {
        return T(rand64());
    }

private:
    uint64_t rand64() noexcept
    {
        m_seed ^= m_seed >> 12, m_seed ^= m_seed << 25, m_seed ^= m_seed >> 27; // NOLINT
        return m_seed * 2685821657736338717LL;                                  // NOLINT
    }

    uint64_t m_seed;
};

template<typename T>
T random() noexcept
{
    return PRNG{}.rand<T>();
}

template uint8_t random<uint8_t>() noexcept;
template uint16_t random<uint16_t>() noexcept;
template uint32_t random<uint32_t>() noexcept;
template uint64_t random<uint64_t>() noexcept;

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
