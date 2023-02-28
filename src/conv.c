#include <stdint.h>

#if defined(__arm__)
uint16_t i_bswap16(uint16_t val)
{
    asm("rev16 r0, r0");
}

uint32_t i_bswap32(uint32_t val)
{
    asm("rev r0, r0");
}

uint64_t i_bswap64(uint64_t val)
{
    asm(
        "mov r1, r2"
        "rev r0, r1"
        "rev r2, r0");
}
#else

#define SWAP(val, type)                             \
    type tmp     = val;                             \
    uint8_t* src = (uint8_t*)&tmp;                  \
    uint8_t* dst = (uint8_t*)&val;                  \
    for (uint_fast8_t i = 0; i < sizeof(type); ++i) \
    {                                               \
        dst[i] = src[sizeof(type) - 1 - i];         \
    }                                               \
    return val;

uint16_t i_bswap16(uint16_t val) {
    SWAP(val, uint16_t)
}

uint32_t i_bswap32(uint32_t val) {
    SWAP(val, uint32_t)
}

uint64_t i_bswap64(uint64_t val)
{
    SWAP(val, uint64_t)
}

#endif