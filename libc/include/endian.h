#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <stdint.h>

/* RISC-V is little-endian */
#define __BYTE_ORDER    __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __PDP_ENDIAN    3412

#define BYTE_ORDER    __BYTE_ORDER
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN    __BIG_ENDIAN
#define PDP_ENDIAN    __PDP_ENDIAN

static inline uint16_t __bswap16(uint16_t x) {
  return (uint16_t)((x >> 8) | (x << 8));
}
static inline uint32_t __bswap32(uint32_t x) {
  return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}
static inline uint64_t __bswap64(uint64_t x) {
  return ((uint64_t)__bswap32((uint32_t)x) << 32) | __bswap32((uint32_t)(x >> 32));
}

/* Host (little-endian) <-> big-endian */
#define htobe16(x) __bswap16(x)
#define htobe32(x) __bswap32(x)
#define htobe64(x) __bswap64(x)
#define be16toh(x) __bswap16(x)
#define be32toh(x) __bswap32(x)
#define be64toh(x) __bswap64(x)

/* Host (little-endian) <-> little-endian (no-ops) */
#define htole16(x) ((uint16_t)(x))
#define htole32(x) ((uint32_t)(x))
#define htole64(x) ((uint64_t)(x))
#define le16toh(x) ((uint16_t)(x))
#define le32toh(x) ((uint32_t)(x))
#define le64toh(x) ((uint64_t)(x))

#endif
