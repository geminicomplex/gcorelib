#ifndef LIBGCORE_H
#define LIBGCORE_H

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "../driver/gemini_core.h"

#ifndef die
#define die(fmt, ...) do{ fprintf(stderr, "%s:%d:%s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); exit(EXIT_FAILURE); } while(0);
#endif

#define MMAP_PATH "/dev/gcore"
#define MMAP_SIZE (DMA_SIZE * sizeof(uint8_t))

enum gcore_wait {
    GCORE_WAIT_NONE = 0,
    GCORE_WAIT_TX = (1 << 0),
    GCORE_WAIT_RX = (1 << 1),
    GCORE_WAIT_BOTH = (1 << 1) | (1 << 0),
};

/*
 * Initialization
 */
void gcore_init();
void gcore_exit();

/*
 * Subcore
 */
void gcore_subcore_load(struct gcore_cfg* gcfg);
void gcore_subcore_run();
void gcore_subcore_idle();
void gcore_subcore_mode_state(uint32_t *mode_state);
void gcore_subcore_reset();

/*
 * Crtl Axi
 */

void gcore_ctrl_write(struct gcore_ctrl_packet *packet);
void gcore_ctrl_read(struct gcore_ctrl_packet *packet);

/*
 * DMA
 */
void *gcore_dma_alloc(int length, int byte_num);
void gcore_dma_alloc_reset(void);
void gcore_dma_prep(
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);
void gcore_dma_start(enum gcore_wait wait);
void gcore_dma_stop();

/*
 * Direct access to axi dma registers.
 *
 */

void dma_mm2s_status();
void dma_s2mm_status();
void dma_status();

/*
 * Helper
 */
struct gcore_registers *gcore_get_regs();
void gcore_dma_prep_start(enum gcore_wait wait,
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);   

#ifdef __cplusplus
}
#endif
#endif				/* LIBGCORE_H */
