#include "util.h"

#include <cstdint>

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
