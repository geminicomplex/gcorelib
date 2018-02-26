/*
 * Portable utility functions
 *
 */

#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "lib/jsmn/jsmn.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef die
#define die(fmt, ...) do{ fprintf(stderr, "%s:%d:%s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); exit(1); } while(0); 
#endif

int util_fopen(const char *file_path, int *fd, FILE **fp, off_t *file_size);
uint64_t* util_get_rand_data(size_t num_bytes);
uint64_t* util_get_static_data(size_t num_bytes, bool include_xor_data, bool clear_xor_results);
uint64_t* util_get_inc_data(size_t num_bytes);
int util_jsmn_eq(const char *json, jsmntok_t *tok, const char *s);
const char *util_get_file_ext_by_path(const char *path);
struct gcore_registers* gcore_get_regs();

#ifdef __cplusplus
}
#endif
#endif
