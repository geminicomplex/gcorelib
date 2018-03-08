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
 */
struct dots {
    // public
    uint32_t num_dots_vecs;
    struct dots_vec **dots_vecs;
    uint32_t cur_dots_vec;
    
    // private
    uint32_t cur_appended_dots_vec;
};

struct dots *create_dots(uint32_t num_dots_vecs);
void append_dots_vec_by_vec_str(struct dots *dots, 
    const char *repeat, const char *vec_str);
void expand_dots_vec_str(struct dots_vec *dots_vec, enum subvecs *data_subvecs, 
    uint32_t num_data_subvecs);
struct dots_vec *get_dots_vec_by_real_id(struct dots *dots, uint32_t id);
struct dots_vec *create_dots_vec(void);
struct dots *free_dots(struct dots *dots);
struct dots_vec *free_dots_vec(struct dots_vec *dots_vec);


#ifdef __cplusplus
}
#endif
#endif
