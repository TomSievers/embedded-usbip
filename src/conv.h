#pragma once

#include <stdint.h>

uint16_t i_bswap16(uint16_t val);
uint32_t i_bswap32(uint32_t val);
uint64_t i_bswap64(uint64_t val);

void* i_swapcpy16(void* buf, uint16_t val);
void* i_swapcpy32(void* buf, uint32_t val);
void* i_swapcpy64(void* buf, uint64_t val);

#ifdef ENDIAN_CONV
#define FROM_NETWORK_ENDIAN_U16(val)           i_bswap16((uint16_t)val)
#define FROM_NETWORK_ENDIAN_U32(val)           i_bswap32((uint32_t)val)
#define FROM_NETWORK_ENDIAN_U64(val)           i_bswap64((uint64_t)val)
#define TO_NETWORK_ENDIAN_U16(val)             FROM_NETWORK_ENDIAN_U16(val)
#define TO_NETWORK_ENDIAN_U32(val)             FROM_NETWORK_ENDIAN_U32(val)
#define TO_NETWORK_ENDIAN_U64(val)             FROM_NETWORK_ENDIAN_U64(val)
#define WRITE_BUF_NETWORK_ENDIAN_U16(buf, val) i_swapcpy16(buf, val)
#define WRITE_BUF_NETWORK_ENDIAN_U32(buf, val) i_swapcpy32(buf, val)
#define WRITE_BUF_NETWORK_ENDIAN_U64(buf, val) i_swapcpy64(buf, val)
#define READ_BUF_NETWORK_ENDIAN_U16(buf, val)  WRITE_BUF_NETWORK_ENDIAN_U16(buf, val)
#define READ_BUF_NETWORK_ENDIAN_U32(buf, val)  WRITE_BUF_NETWORK_ENDIAN_U32(buf, val)
#define READ_BUF_NETWORK_ENDIAN_U64(buf, val)  WRITE_BUF_NETWORK_ENDIAN_U64(buf, val)
#else
#define FROM_NETWORK_ENDIAN_U16(val)           val
#define FROM_NETWORK_ENDIAN_U32(val)           val
#define FROM_NETWORK_ENDIAN_U64(val)           val
#define TO_NETWORK_ENDIAN_U16(val)             val
#define TO_NETWORK_ENDIAN_U32(val)             val
#define TO_NETWORK_ENDIAN_U64(val)             val
#define WRITE_BUF_NETWORK_ENDIAN_U16(buf, val) memcpy(buf, (uint8_t*)&val, sizeof(uint16_t))
#define WRITE_BUF_NETWORK_ENDIAN_U32(buf, val) memcpy(buf, (uint8_t*)&val, sizeof(uint32_t))
#define WRITE_BUF_NETWORK_ENDIAN_U64(buf, val) memcpy(buf, (uint8_t*)&val, sizeof(uint64_t))
#define READ_BUF_NETWORK_ENDIAN_U16(buf, val)  WRITE_BUF_NETWORK_ENDIAN_U16(buf, val)
#define READ_BUF_NETWORK_ENDIAN_U32(buf, val)  WRITE_BUF_NETWORK_ENDIAN_U32(buf, val)
#define READ_BUF_NETWORK_ENDIAN_U64(buf, val)  WRITE_BUF_NETWORK_ENDIAN_U64(buf, val)
#endif