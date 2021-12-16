/*
 * Common
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif


#include "lib/slog/slog.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef VERILATOR
#define GEM_DEBUG
#else
#define GEM_DEBUG
#endif

// can't include kernel headers so add defines here
#ifndef u32
    #define u32 uint32_t
#endif
#ifndef dma_cookie_t
    #define dma_cookie_t int32_t
#endif

#define GCORE_LOG_DIR "/var/log"
#define GCORE_LOG_NAME "gemini"

// stack levels to print when dying
#define MAX_STACK_LEVELS (100)

// declare prototype
void print_stacktrace();

#ifndef bye
#define bye(fmt, ...) do{\
    fprintf(stderr, fmt, ##__VA_ARGS__);\
    exit(EXIT_FAILURE);\
    } while(0);
#endif

#ifndef die
#define die(fmt, ...) do{\
    slog_fatal(fmt, ##__VA_ARGS__);\
    print_stacktrace();\
    exit(EXIT_FAILURE);\
    } while(0);
#endif

// total artix memory in bytes (8GiB)
#define ARTIX_MEM_BYTES (8589934592)

// max size we can send due to zynq memory limitations
#define MAX_CHUNK_SIZE (536870912)

// number of bytes per dma burst also known as a memory page
// a word is a 64 bit number
#define BURST_BYTES (1024)
#define BEAT_BYTES (128)
#define NUM_BEATS_PER_BURST (8)
#define WORD_BYTES (8)
#define NUM_WORDS_PER_BEAT (16)

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

// maximum number of vectors we can fit in 8GiB memory
#define MAX_NUM_VECS (67108864)

// maximum number of unrolled vectors. 64 bit number, but 
// arbitrarily limit it to (2**37), which is around 45 mins
// runtime at 20 ns per vec.
#define MAX_NUM_UNROLLED_VECS (137438953472)

// (2**35) vecs. Runtime around 11 mins at 20 ns per vec
#define MAX_NUM_UNROLLED_VECS_WARNING (34359738368)

// len=0x0f, size 2**6=64, incr_mode=0x1
#define MEMCORE_BURST_CFG (0x0000f610)

/*
 * Log default is debug off.
 */
void gcore_init();

/*
 * Enables all log levels including: SLOG_LIVE, SLOG_DEBUG, SLOG_TRACE
 */
void gcore_enable_log_debug();

/*
 * Enables all log levels except: SLOG_LIVE, SLOG_DEBUG, SLOG_TRACE
 */
void gcore_disable_log_debug();

#ifdef __cplusplus
}
#endif
#endif
