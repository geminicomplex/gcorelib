#ifndef LIBGCORE_H
#define LIBGCORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "../driver/gemini_core.h"

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
int gcore_init();
int gcore_exit(void);

/*
 * Subcore
 */
int gcore_subcore_load(struct gcore_cfg* gcfg);
int gcore_subcore_run();
int gcore_subcore_idle();
int gcore_subcore_mode_state(uint32_t *mode_state);
int gcore_subcore_reset();

/*
 * Crtl Axi
 */

int gcore_ctrl_write(struct gcore_ctrl_packet *packet);
int gcore_ctrl_read(struct gcore_ctrl_packet *packet);

/*
 * DMA
 */
void *gcore_dma_alloc(int length, int byte_num);
void gcore_dma_alloc_reset(void);
int gcore_dma_prep(
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);
int gcore_dma_start(enum gcore_wait wait);
int gcore_dma_stop();

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
 int gcore_dma_prep_start(enum gcore_wait wait,
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);   

#ifdef __cplusplus
}
#endif
#endif				/* LIBGCORE_H */
