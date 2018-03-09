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
void gcore_subcore_load(struct gcore_cfg* gcfg);
void gcore_subcore_run(void);
void gcore_subcore_idle(void);
void gcore_subcore_mode_state(uint32_t *mode_state);
void gcore_subcore_reset(void);

/*
 * Crtl Axi
 */

void gcore_ctrl_write(struct gcore_ctrl_packet *packet);
void gcore_ctrl_read(struct gcore_ctrl_packet *packet);

/*
 * Registers
 *
 */
struct gcore_registers* gcore_get_regs(void);
struct gcore_registers* gcore_free_regs(struct gcore_registers *regs);

#ifdef __cplusplus
}
#endif
#endif				/* SUBCORE_H */
