/*
 * Dots parser
 *
 */

#include "dots.h"
#include "util.h"
#include "profile.h"
#include "stim.h"

#include "../driver/gemini_core.h"

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
 * Opens a dots file, checks extension and that it's readable, create the structs.
 * Need to manually call append_dots_vec_by_vec_str. Don't want parsing to be done
 * in C.
 *
 */
struct dots *open_dots(const char *profile_path, const char *path, uint32_t num_dots_vecs){
    struct dots * dots = NULL;
    int fd;
    FILE *fp = NULL;
    off_t file_size;
    struct profile *profile = NULL;

    profile = get_profile_by_path(profile_path);
    if(profile == NULL){
        die("error: pointer is NULL\n");
    }

    if(util_fopen(path, &fd, &fp, &file_size)){
        die("error: failed to open file '%s'\n", path);
    }
    
    const char *file_ext = util_get_file_ext_by_path(path);
    if(strcmp(file_ext, "s") != 0){
        die("error: invalid file type given '%s'\n", file_ext);
    }

    if((dots = create_dots(num_dots_vecs)) == NULL){
        die("error: pointer is NULL\n");
    }

    return dots;
}

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

    if(dots->cur_dots_vec >= dots->num_dots_vecs){
        die("error: failed to append dots vec exceeded \
            allocated limit of %i", dots->num_dots_vecs);
    }

    dots_vec = create_dots_vec();
    dots_vec->repeat = atoi(repeat);
    dots_vec->vec_str = strdup(vec_str);
    dots->dots_vecs[dots->cur_dots_vec++] = dots_vec;

    return;
}

/*
 * Expands a vector string into an array of un-packed subvecs. 
 *
 */
void fill_dots_vec(struct dots_vec *dots_vec, enum subvecs *data_subvecs, 
        uint32_t num_data_subvecs){
    uint32_t num_pins = get_config_num_profile_pins();
    uint32_t vec_str_len = 0;

    if(dots_vec == NULL){
        die("error: pointer is NULL\n");
    }

    if(dots_vec->is_filled){
        die("error: dots_vec is already filled\n");
    }

    if(dots_vec->vec_str == NULL){
        die("error: pointer is NULL\n");
    }

    // len should only be config pins except data pins
    vec_str_len = strlen(dots_vec->vec_str);

    if((vec_str_len+PROFILE_NUM_DATA_PINS) != num_pins){
        die("error: (vec_len + num_data_pins) %i != "
            "num_pins %i\n", (vec_str_len+PROFILE_NUM_DATA_PINS), num_pins);
    }

    if(dots_vec->num_subvecs != num_pins){
        die("error: num subvecs for dots_vec %i != "
            "num_pins %i\n", dots_vec->num_subvecs, num_pins);
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
        if((vec_str_len+num_data_subvecs) != num_pins){
            die("error: num_subvecs %i != num_pins %i\n", (vec_str_len+num_data_subvecs), num_pins);
        }
        // fill rest of subvecs, but start at index 0 for data_subvecs
        int j = 0;
        for(int i=vec_str_len; i<num_pins; i++){
            dots_vec->subvecs[i] = data_subvecs[j++];
        }
    // for header or footer just don't drive on the D pins
    }else{
        for(int i=vec_str_len; i<num_pins; i++){
            dots_vec->subvecs[i] = DUT_SUBVEC_X;
        }
    }

    dots_vec->is_filled = true;

    return;
}


/*
 * Config stores the vectors in a compressed form based on the repeat value.
 * Return which vector bucket the id falls into.
 *
 */
struct dots_vec *get_dots_vec_by_real_id(struct dots *dots, uint32_t id){
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
 * Allocates a new dots object.
 *
 */
struct dots *create_dots(uint32_t num_dots_vecs){
    struct dots *dots = NULL;
    if((dots = (struct dots*)malloc(sizeof(struct dots))) == NULL){
        die("error: failed to malloc struc\n");
    }
    dots->cur_dots_vec = 0;
    dots->num_dots_vecs = num_dots_vecs;

    // always store compressed vecs 
    if((dots->dots_vecs = (struct dots_vec**)calloc(dots->num_dots_vecs, sizeof(struct dots_vec*))) == NULL){
        die("error: failed to calloc dots_vecs\n");
    }

    return dots;
}

/*
 * Allocates a new dots_vec object.
 *
 */
struct dots_vec *create_dots_vec(){
    struct dots_vec *dots_vec;
    uint32_t num_pins = get_config_num_profile_pins();
    if((dots_vec = (struct dots_vec*)malloc(sizeof(struct dots_vec))) == NULL){
        die("error: failed to malloc struc\n");
    }
    dots_vec->repeat = 0;
    dots_vec->vec_str = NULL;
    dots_vec->is_filled = false;
    dots_vec->num_subvecs = num_pins;
    dots_vec->subvecs = NULL;

    if((dots_vec->subvecs = (enum subvecs *)calloc(dots_vec->num_subvecs, sizeof(enum subvecs))) == NULL){
        die("error: failed to calloc dots_vecs' vecs\n");
    }

    for(int i=0; i<dots_vec->num_subvecs; i++){
        dots_vec->subvecs[i] = DUT_SUBVEC_X;
    }

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
    
    for(int i=0; i<dots->cur_dots_vec; i++){
        dots->dots_vecs[i] = free_dots_vec(dots->dots_vecs[i]);
    }
    free(dots->dots_vecs);
    dots->cur_dots_vec = 0;
    dots->num_dots_vecs = 0;
    dots->dots_vecs = NULL;
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
    dots_vec->is_filled = false;
    dots_vec->num_subvecs = 0;
    free(dots_vec->subvecs);
    dots_vec->subvecs = NULL;
    free(dots_vec);
    return NULL;
}
