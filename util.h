/*
 * Utility Helper Functions
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#include "lib/jsmn/jsmn.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

int util_fopen(const char *file_path, int *fd, FILE **fp, off_t *file_size);
uint64_t* util_get_rand_data(size_t num_bytes, uint32_t seed);
uint64_t* util_get_static_data(size_t num_bytes, bool include_xor_data, bool clear_xor_results);
uint64_t* util_get_inc_data(size_t num_bytes);
int util_jsmn_eq(const char *json, jsmntok_t *tok, const char *s);
const char *util_get_file_ext_by_path(const char *path);
size_t util_str_count(const char* s, char c);
char *util_str_strip(char *s);
size_t util_str_split(char* a_str, const char a_delim, char*** results);

#ifdef __cplusplus
}
#endif
#endif
