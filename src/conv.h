#pragma once

#include <stdint.h>

uint16_t i_bswap16(uint16_t val);
uint32_t i_bswap32(uint32_t val);
uint64_t i_bswap64(uint64_t val);

#ifdef ENDIAN_CONV
#define FROM_NETWORK_ENDIAN_U16(val) i_bswap16((uint16_t)val)
#define FROM_NETWORK_ENDIAN_U32(val) i_bswap32((uint32_t)val)
#define FROM_NETWORK_ENDIAN_U64(val) i_bswap64((uint64_t)val)
#define TO_NETWORK_ENDIAN_U16(val)   i_bswap16((uint16_t)val)
#define TO_NETWORK_ENDIAN_U32(val)   i_bswap32((uint32_t)val)
#define TO_NETWORK_ENDIAN_U64(val)   i_bswap64((uint64_t)val)
#else
#define FROM_NETWORK_ENDIAN_U16(val) val
#define FROM_NETWORK_ENDIAN_U32(val) val
#define FROM_NETWORK_ENDIAN_U64(val) val
#define TO_NETWORK_ENDIAN_U16(val)   val
#define TO_NETWORK_ENDIAN_U32(val)   val
#define TO_NETWORK_ENDIAN_U64(val)   val
#endif