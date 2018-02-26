/*
 * Dots parser
 *
 */

#ifndef DOTS_H
#define DOTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../driver/gemini_core.h"

/*
 * Holds the vector string with the repeat.
 *
 */
struct dots_vec {
    uint32_t repeat;
    char *vec_str;
    bool is_filled;
    uint32_t num_subvecs;
    enum subvecs *subvecs;
    bool has_clk;
};

/*
 * Struct represents a parsed dots file.
 *
 */
struct dots {
    uint32_t cur_dots_vec;
    uint32_t num_dots_vecs;
    struct dots_vec **dots_vecs;
};

struct dots *open_dots(const char *profile_path, const char *path,
    uint32_t num_dots_vecs);
void append_dots_vec_by_vec_str(struct dots *dots, 
    const char *repeat, const char *vec_str);
void fill_dots_vec(struct dots_vec *dots_vec, enum subvecs *data_subvecs, 
    uint32_t num_data_subvecs);
struct dots_vec *get_dots_vec_by_real_id(struct dots *dots, uint32_t id);
struct dots *create_dots(uint32_t num_dots_vecs);
struct dots_vec *create_dots_vec();
struct dots *free_dots(struct dots *dots);
struct dots_vec *free_dots_vec(struct dots_vec *dots_vec);


#ifdef __cplusplus
}
#endif
#endif
