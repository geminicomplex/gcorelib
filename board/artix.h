/*
 * Artix
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef ARTIX_H
#define ARTIX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver.h"
#include "../stim.h"

void artix_mem_write(enum artix_selects artix_select,
    uint64_t addr, uint64_t *write_data, size_t write_size);
void artix_mem_read(enum artix_selects artix_select, uint64_t addr,
    uint64_t *read_data, size_t read_size);
// if full test true, will run full 8GiB test
void artix_mem_test(enum artix_selects artix_select, bool run_crc, bool full_test);
// note: if stim is dual pattern, this will load both A1 and A2 at same addr
uint64_t artix_load_stim(struct stim *stim, uint64_t load_addr);
bool artix_run_stim(struct stim *stim, uint64_t *test_cycle, uint64_t start_addr);
void artix_get_stim_fail_pins(uint8_t **fail_pins, uint32_t *num_fail_pins);
void artix_print_stim_fail_pins(struct stim *stim, uint8_t *fail_pins, 
    uint32_t num_fail_pins);
void artix_config(enum artix_selects artix_select, const char *bit_path);

#ifdef __cplusplus
}
#endif
#endif  /* ARTIX_H */
