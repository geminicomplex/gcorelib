/*
 * Dots parser
 *
 */

#ifndef DOTS_H
#define DOTS_H

#ifdef __cplusplus
extern "C" {
#endif

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "profile.h"
#include "subvec.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * dots vector
 *
 */
struct dots_vec {
    // vector repeat count and stimulus string
    uint32_t repeat;
    char *vec_str;

    // has the repeat/vec_str been converted to subvecs
    bool is_expanded;

    // packed subvecs and length
    uint32_t num_subvecs;
    enum subvecs *subvecs;

    // does the vector have a clock pin
    bool has_clk;
};

/*
 * dots
 *
 * num_dots_vecs: len of dots_vecs
 * dots_vecs: array of dots_vecs
 * num_pins: len of pins
 * cur_a1_dots_vec_id: current chunk read index
 * cur_a2_dots_vec_id: current chunk read index
 *
 */
struct dots {
    // public
    uint32_t num_dots_vecs;
    struct dots_vec **dots_vecs;
    uint16_t num_pins;
    struct profile_pin **pins;
    uint32_t cur_a1_dots_vec_id;
    uint32_t cur_a2_dots_vec_id;
    
    // private
    uint32_t cur_appended_dots_vec_id;
};

struct dots *create_dots(uint32_t num_dots_vecs, struct profile_pin **pins, 
    uint32_t num_pins);
void append_dots_vec_by_vec_str(struct dots *dots, 
    const char *repeat, const char *vec_str);
void append_dots_vec_by_nop_vecs(struct dots *dots, uint32_t num_nop_vecs);
void expand_dots_vec_subvecs(struct dots *dots, struct dots_vec *dots_vec, 
    enum subvecs *data_subvecs, uint32_t num_data_subvecs);
void unexpand_dots_vec_subvecs(struct dots_vec *dots_vec);
struct dots_vec *get_dots_vec_by_unrolled_id(struct dots *dots, uint32_t id);
uint32_t get_num_unrolled_dots_vecs(struct dots *dots);
struct dots_vec *create_dots_vec(struct dots *dots);
struct dots *free_dots(struct dots *dots);
struct dots_vec *free_dots_vec(struct dots_vec *dots_vec);


#ifdef __cplusplus
}
#endif
#endif
