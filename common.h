#ifndef COMMON_H
#define COMMON_H

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif
#endif
