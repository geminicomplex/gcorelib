/*
 * Dots parser
 *
 */

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "dots.h"
#include "util.h"
#include "profile.h"
#include "stim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>


/*
 * Creates a new dots_vec based on a vec string and appends it to the
 * dots_vecs array.
 *
 */
void append_dots_vec_by_vec_str(struct dots *dots, 
        const char *repeat, const char *vec_str){
    struct dots_vec *dots_vec = NULL;
    if(dots == NULL){
        die("error: pointer is NULL\n");
    }
    if(repeat == NULL){
        die("error: pointer is NULL\n");
    }
    if(vec_str == NULL){
        die("error: pointer is NULL\n");
    }

    if(dots->cur_appended_dots_vec_id >= dots->num_dots_vecs){
        die("error: failed to append dots vec exceeded \
            allocated limit of %i", dots->num_dots_vecs);
    }

    dots_vec = create_dots_vec(dots);
    dots_vec->repeat = atoi(repeat);
    dots_vec->vec_str = strdup(vec_str);
    dots->dots_vecs[dots->cur_appended_dots_vec_id++] = dots_vec;

    return;
}

/*
 * Expands a vector string into an array of un-packed subvecs. 
 *
 */
void expand_dots_vec_subvecs(struct dots *dots, struct dots_vec *dots_vec, enum subvecs *data_subvecs, 
        uint32_t num_data_subvecs){
    uint32_t vec_str_len = 0;

    if(dots_vec == NULL){
        die("error: pointer is NULL\n");
    }

    if(dots_vec->is_expanded){
        die("error: dots_vec is already filled\n");
    }

    if(dots_vec->vec_str == NULL){
        die("error: pointer is NULL\n");
    }

    // len should only be config pins except data pins
    vec_str_len = (uint32_t)strlen(dots_vec->vec_str);

    // if dots represents a bitstream, check if vec length accounts for
    // the data pins
    if(data_subvecs != NULL){
        if((vec_str_len+PROFILE_NUM_DATA_PINS) != dots->num_pins){
            die("error: (vec_len + num_data_pins) %i != "
                "num_pins %i\n", (vec_str_len+PROFILE_NUM_DATA_PINS), dots->num_pins);
        }
    }

    if(dots_vec->num_subvecs != dots->num_pins){
        die("error: num subvecs for dots_vec %i != "
            "num_pins %i\n", dots_vec->num_subvecs, dots->num_pins);
    }

    if(dots_vec->subvecs != NULL){
        die("failed to expand dots_vec; subvecs is already allocated");
    }

    if((dots_vec->subvecs = (enum subvecs *)calloc(dots_vec->num_subvecs, sizeof(enum subvecs))) == NULL){
        die("error: failed to calloc dots_vecs' vecs\n");
    }

    for(int i=0; i<dots_vec->num_subvecs; i++){
        dots_vec->subvecs[i] = DUT_SUBVEC_X;
    }

    for(int i=0; i<vec_str_len; i++){
        switch(dots_vec->vec_str[i]){
            case '0':
                dots_vec->subvecs[i] = DUT_SUBVEC_0; 
                break;
            case '1':
                dots_vec->subvecs[i] = DUT_SUBVEC_1; 
                break;
            case 'X':
                dots_vec->subvecs[i] = DUT_SUBVEC_X; 
                break;
            case 'H':
                dots_vec->subvecs[i] = DUT_SUBVEC_H; 
                break;
            case 'L':
                dots_vec->subvecs[i] = DUT_SUBVEC_L; 
                break;
            case 'C':
                dots_vec->subvecs[i] = DUT_SUBVEC_C;
                dots_vec->has_clk = true;
                break;
        }
    }

    // for body, inject the data subvecs from the stim
    if(data_subvecs != NULL){
        if((vec_str_len+num_data_subvecs) != dots->num_pins){
            die("error: num_subvecs %i != num_pins %i\n", (vec_str_len+num_data_subvecs), dots->num_pins);
        }
        // fill rest of subvecs, but start at index 0 for data_subvecs
        int j = 0;
        for(int i=vec_str_len; i<dots->num_pins; i++){
            dots_vec->subvecs[i] = data_subvecs[j++];
        }
    // for header or footer just don't drive on the D pins
    }else{
        for(int i=vec_str_len; i<dots->num_pins; i++){
            dots_vec->subvecs[i] = DUT_SUBVEC_X;
        }
    }

    dots_vec->is_expanded = true;

    return;
}

/*
 * Frees the dots_vec subvecs memory.
 *
 */
void unexpand_dots_vec_subvecs(struct dots_vec *dots_vec){
    if(dots_vec == NULL){
        die("pointer is NULL\n");
    }

    if(dots_vec->is_expanded == false){
        return;
    }

    if(dots_vec->subvecs != NULL){
        free(dots_vec->subvecs);
        dots_vec->subvecs = NULL; 
    }

    dots_vec->is_expanded = false;

    return;
}


/*
 * Config stores the vectors in a compressed form based on the repeat value.
 * Return which vector bucket the id falls into.
 *
 */
struct dots_vec *get_dots_vec_by_unrolled_id(struct dots *dots, uint32_t id){
    struct dots_vec *dots_vec = NULL;
    uint32_t start = 0;
    if(dots == NULL){
        die("error: pointer is NULL\n");
    }
    for(int i=0; i<dots->num_dots_vecs; i++){
        struct dots_vec *v = dots->dots_vecs[i];
        if((id >= start) && (id < start+v->repeat)){
            dots_vec = v;
            break;
        }
        start += v->repeat;
    }
    return dots_vec;
}

/*
 * Iterates through dots_vecs and adds up the repeat
 * value to get the total unrolled vec count.
 *
 */
uint32_t get_num_unrolled_dots_vecs(struct dots *dots){
    uint32_t num_unrolled_vecs = 0;

    if(dots == NULL){
        die("pointer is NULL\n");
    }

    for(int i=0; i<dots->num_dots_vecs; i++){
        if(dots->dots_vecs[i] == NULL){
            die("pointer is NULL\n");
        }
        num_unrolled_vecs += dots->dots_vecs[i]->repeat;
    }

    return num_unrolled_vecs;
}


/*
 * Allocates a new dots object. Pins and num_pins are optional and are usually
 * not set if the dots was generated for a bitstream by config.c.
 * 
 *
 */
struct dots *create_dots(uint32_t num_dots_vecs, struct profile_pin **pins, 
        uint32_t num_pins){
    struct dots *dots = NULL;

    if(pins == NULL){
        die("pointer is NULL\n");
    }

    if(num_pins == 0){
        die("cannot create a dots with zero pins\n");
    }

    if((dots = (struct dots*)malloc(sizeof(struct dots))) == NULL){
        die("error: failed to malloc struc\n");
    }
    dots->num_dots_vecs = num_dots_vecs;

    // always store compressed vecs 
    if((dots->dots_vecs = (struct dots_vec**)calloc(dots->num_dots_vecs, sizeof(struct dots_vec*))) == NULL){
        die("error: failed to calloc dots_vecs\n");
    }

    dots->num_pins = num_pins;

    if((dots->pins = (struct profile_pin **)calloc(dots->num_pins, sizeof(struct profile_pin*))) == NULL){
        die("error: failed to calloc profile pins\n");
    }

    for(uint32_t i=0; i<dots->num_pins; i++){
        dots->pins[i] = create_profile_pin(pins[i], pins[i]->num_dests);
    }

    dots->cur_appended_dots_vec_id = 0;

    return dots;
}

/*
 * Allocates a new dots_vec object.
 *
 */
struct dots_vec *create_dots_vec(struct dots *dots){
    struct dots_vec *dots_vec;

    if(dots == NULL){
        die("pointer is NULL\n");
    }

    if((dots_vec = (struct dots_vec*)malloc(sizeof(struct dots_vec))) == NULL){
        die("error: failed to malloc struc\n");
    }
    dots_vec->repeat = 0;
    dots_vec->vec_str = NULL;
    dots_vec->is_expanded = false;
    dots_vec->num_subvecs = dots->num_pins;
    dots_vec->subvecs = NULL;

    dots_vec->has_clk = false;

    return dots_vec;
}

/*
 * De-allocates dots struct and internal members.
 *
 */
struct dots *free_dots(struct dots *dots){
    if(dots == NULL){
        die("error: pointer is NULL\n");
    }
    
    for(uint32_t i=0; i<dots->cur_appended_dots_vec_id; i++){
        dots->dots_vecs[i] = free_dots_vec(dots->dots_vecs[i]);
    }
    free(dots->dots_vecs);
    dots->dots_vecs = NULL;
    dots->num_dots_vecs = 0;
    dots->cur_appended_dots_vec_id = 0;

    if(dots->pins != NULL){
        for(uint32_t i=0; i<dots->num_pins; i++){
            free_profile_pin(dots->pins[i]);
        }
        free(dots->pins);
    }
    dots->pins = NULL;
    dots->num_pins = 0;
    free(dots);
    return NULL;
}


/*
 * De-allocates config vec struct and internal members.
 *
 */
struct dots_vec *free_dots_vec(struct dots_vec *dots_vec){
    if(dots_vec == NULL){
        die("error: pointer is NULL\n");
    }
    dots_vec->repeat = 0;
    free(dots_vec->vec_str);
    dots_vec->vec_str = NULL;
    dots_vec->is_expanded = false;
    dots_vec->num_subvecs = 0;
    if(dots_vec->subvecs != NULL){
        free(dots_vec->subvecs);
        dots_vec->subvecs = NULL;
    }
    free(dots_vec);
    return NULL;
}
