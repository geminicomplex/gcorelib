/*
 * Gemini helper
 *
 */

#ifndef HELPER_H
#define HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../driver/gcore_common.h"

#define ARTIX_READ_FIFO_BYTES (2097152)

void helper_num_bursts_load(enum artix_selects artix_select,
    uint32_t num_bursts);
void helper_subcore_load(enum artix_selects artix_select,
    enum subcore_states subcore_state);
void helper_agent_load(enum artix_selects artix_select,
    enum agent_states agent_state);
void helper_memcore_load(enum artix_selects artix_select,
    enum memcore_states memcore_state);
void helper_memcore_check_state(enum artix_selects artix_select, 
        enum memcore_states memcore_state, uint32_t num_bursts);
void helper_gvpu_load(enum artix_selects artix_select,
    enum gvpu_states gvpu_state);
void helper_gvpu_packet_write(enum artix_selects artix_select,
    struct gcore_ctrl_packet *packet);
uint64_t helper_get_agent_status_cycle(enum artix_selects artix_select);
void helper_get_agent_status(enum artix_selects artix_select, struct gcore_ctrl_packet *packet);
void helper_print_agent_status(enum artix_selects artix_select);
void helper_fopen(char *file_path, int *fd, FILE **fp, off_t *file_size);
void sprint_subcore_mode_state(char *mode_state_str);
void print_regs(struct gcore_registers *regs);
void print_packet(struct gcore_ctrl_packet *packet, char *pre);
void print_agent_state(enum agent_states agent_state, char *pre);
void print_gvpu_state(enum gvpu_states gvpu_state, char *pre);
void print_memcore_state(enum memcore_states memcore_state, char *pre);

#ifdef __cplusplus
}
#endif
#endif
