#ifndef SUBCORE_H
#define SUBCORE_H

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../driver/gcore_common.h"

/*
 * Subcore
 */
void subcore_load(struct gcore_cfg* gcfg);
void subcore_run(void);
void subcore_idle(void);
void subcore_mode_state(uint32_t *mode_state);
void subcore_reset(void);

/*
 * Crtl Axi
 */

void subcore_write_packet(struct gcore_ctrl_packet *packet);
void subcore_read_packet(struct gcore_ctrl_packet *packet);

/*
 * Artix Ctrl
 */
void subcore_artix_sync(bool sync);

/*
 * Registers
 *
 */
struct gcore_registers* subcore_get_regs(void);
struct gcore_registers* subcore_free_regs(struct gcore_registers *regs);

#ifdef __cplusplus
}
#endif
#endif				/* SUBCORE_H */
