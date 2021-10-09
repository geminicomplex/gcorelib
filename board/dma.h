/*
 * DMA through gcore
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */
#ifndef DMA_H
#define DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "driver.h"

#define BUS_IN_BYTES 8
#define BUS_BURST 16

// AXI DMA register offsets
#define DMA_BASE_ADDR 0x40400000
#define MM2S_CONTROL 0x00
#define MM2S_STATUS 0x04
#define MM2S_CURDESC 0x08
#define MM2S_CURDESC_MSB 0x0C
#define MM2S_TAILDESC 0x10
#define MM2S_TAILDESC_MSB 0x14
#define MM2S_START_ADDR 0x18
#define MM2S_LENGTH 0x28
#define S2MM_CONTROL 0x30
#define S2MM_STATUS 0x34
#define S2MM_CURDESC 0x38
#define S2MM_CURDESC_MSB 0x3C
#define S2MM_TAILDESC 0x40
#define S2MM_TAILDESC_MSB 0x44
#define S2MM_DEST_ADDR 0x48
#define S2MM_LENGTH 0x58

enum gcore_wait {
    GCORE_WAIT_NONE = 0,
    GCORE_WAIT_TX = (1 << 0),
    GCORE_WAIT_RX = (1 << 1),
    GCORE_WAIT_BOTH = (1 << 1) | (1 << 0),
};

/*
 * DMA
 */
void *gcore_dma_alloc(int length, int byte_num);
void gcore_dma_alloc_reset(void);
void gcore_dma_prep(
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);
void gcore_dma_start(enum gcore_wait wait);
void gcore_dma_prep_start(enum gcore_wait wait,
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);
void gcore_dma_stop();
uint32_t gcore_dma_calc_offset(void *ptr);


/*
 * Direct access to axi dma registers.
 *
 */

void dma_mm2s_status();
void dma_s2mm_status();
void dma_status();


#ifdef __cplusplus
}
#endif
#endif				/* DMA_H */
