#include <cstdint>

#include "util.h"

class PRNG
{
public:
    PRNG(uint64_t seed) : m_seed{ seed } { }

    template<typename T>
    T rand() { return T(rand64()); }

private:
    uint64_t rand64()
    {
        m_seed ^= m_seed >> 12, m_seed ^= m_seed << 25, m_seed ^= m_seed >> 27;
        return m_seed * 2685821657736338717LL;
    }

    uint64_t m_seed;
};

static PRNG prng{ uint64_t(now().time_since_epoch().count()) };

template<typename T>
T random()
{
    return prng.rand<T>();
}

template uint8_t random<uint8_t>();
template uint16_t random<uint16_t>();
template uint32_t random<uint32_t>();
template uint64_t random<uint64_t>();
