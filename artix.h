#ifndef ARTIX_H
#define ARTIX_H

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

// len=0x0f, size 2**6=64, incr_mode=0x1
#define MEMCORE_BURST_CFG (0x0000f610)

void artix_mem_write(enum artix_selects artix_select,
        uint64_t addr, uint64_t *write_data, size_t write_size);
void artix_mem_read(enum artix_selects artix_select, uint64_t addr,
    uint64_t *read_data, size_t read_size);
void artix_mem_test(enum artix_selects artix_select, bool run_crc);
void artix_dut_test(enum artix_selects artix_select, char *profile_path, char *file_path);
void artix_config(enum artix_selects artix_select, const char *bit_path);

#ifdef __cplusplus
}
#endif
#endif				/* ARTIX_H */
