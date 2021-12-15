/*
 * Gemini board helper functions
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef HELPER_H
#define HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver.h"

#define ARTIX_READ_FIFO_BYTES (2097152)

#define GET_START_RANK(addr) ((uint32_t)(((uint64_t)addr & 0x0000000100000000) >> 32)) 
#define GET_START_ADDR(addr) ((uint32_t)((uint64_t)addr & 0x00000000ffffffff))

/*
 * Subcore led types that can be set.
 */
enum subcore_leds {
    SUBCORE_RED_LED,
    SUBCORE_GREEN_LED
};

/*
 * Gvpu status commands
 */

enum gvpu_status_selects {
    GVPU_STATUS_SELECT_NONE = 0x0,
    GVPU_STATUS_SELECT_MEM_TEST = 0x1,
    GVPU_STATUS_SELECT_DUT_TEST = 0x2,
    GVPU_STATUS_SELECT_MEM_RW = 0x3
};

enum gvpu_status_cmds {
    GVPU_STATUS_CMD_NONE = 0x0,
    GVPU_STATUS_CMD_GET_CYCLE = 0x1
};


void helper_subcore_load(enum artix_selects artix_select,
    enum subcore_states subcore_state);
void helper_subcore_set_boot_done();
void helper_subcore_set_led(enum subcore_leds subcore_led, bool on);
uint64_t helper_subcore_get_dna_id();
void helper_agent_load(enum artix_selects artix_select,
    enum agent_states agent_state);
void helper_memcore_load(enum artix_selects artix_select,
    enum memcore_states memcore_state);
void helper_memcore_check_state(enum artix_selects artix_select, 
        enum memcore_states memcore_state, uint32_t num_bursts);
void helper_burst_setup(enum artix_selects artix_select,
    uint64_t start_addr, uint32_t num_bursts);
void helper_gvpu_load(enum artix_selects artix_select,
    enum gvpu_states gvpu_state);
void helper_gvpu_packet_write(enum artix_selects artix_select,
    struct gcore_ctrl_packet *packet);
uint64_t helper_get_agent_gvpu_status(enum artix_selects artix_select, 
        enum gvpu_status_selects select, enum gvpu_status_cmds cmd);
void helper_get_agent_status(enum artix_selects artix_select, struct gcore_ctrl_packet *packet);
void helper_print_agent_status(enum artix_selects artix_select);
void helper_fopen(char *file_path, int *fd, FILE **fp, off_t *file_size);
void sprint_subcore_mode_state(char *mode_state_str);
void print_regs(struct gcore_registers *regs);
void print_regs_verbose(struct gcore_registers *regs);
void print_packet(struct gcore_ctrl_packet *packet, char *pre);
enum agent_states get_agent_state(enum artix_selects artix_select, struct gcore_registers *regs);
void print_agent_state(enum agent_states agent_state, char *pre);
enum gvpu_states get_gvpu_state(enum artix_selects artix_select, struct gcore_registers *regs);
void print_gvpu_state(enum gvpu_states gvpu_state, char *pre);
enum memcore_states get_memcore_state(enum artix_selects artix_select, struct gcore_registers *regs);
void print_memcore_state(enum memcore_states memcore_state, char *pre);
uint32_t get_gvpu_stage(enum artix_selects artix_select, struct gcore_registers *regs);

#ifdef __cplusplus
}
#endif
#endif
