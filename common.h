#ifndef COMMON_H
#define COMMON_H

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../driver/gcore_common.h"

//#define GEM_DEBUG

// can't include kernel headers so add defines here
#ifndef u32
    #define u32 uint32_t
#endif
#ifndef dma_cookie_t
    #define dma_cookie_t int32_t
#endif

#ifndef die
#define die(fmt, ...) do{ fprintf(stderr, "%s:%d:%s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); exit(EXIT_FAILURE); } while(0);
#endif

#define MMAP_PATH "/dev/gcore"
#define MMAP_SIZE (DMA_SIZE * sizeof(uint8_t))

// max size we can send due to memory limitations
#define MAX_CHUNK_SIZE (536870912/2)

// number of bytes per dma burst
#define BURST_BYTES (1024)


// number of pins per artix unit
#define DUT_NUM_PINS (200)

// total number of dut io pins
#define DUT_TOTAL_NUM_PINS (400)
#define DUT_MAX_VECTORS (67108864)


// A vector is 128 bytes or 256 nibbles. A subvec is stored in a nibble and we
// have 200 subvecs for 200 pins. Rest of the 56 nibbles are used for opcode
// and operand.
#define STIM_VEC_SIZE (128)

// 4 byte word, 1024 byte burst and 4096 byte page aligned
#define STIM_CHUNK_SIZE (268435456)

// 8 vecs per 1024 byte burst
#define STIM_NUM_VECS_PER_BURST (8)


#ifdef __cplusplus
}
#endif
#endif
