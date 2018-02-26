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
#include "../driver/gemini_core.h"

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

#ifdef __cplusplus
}
#endif
#endif				/* SUBCORE_H */
