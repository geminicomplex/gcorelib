/*
 * DOTS test pattern
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

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


struct dots *parse_dots(struct profile *profile, char *dots_path){
    int fd;
    FILE *fp = NULL;
    off_t file_size = 0;
    char *file_line = NULL;
    size_t len = 0;
    ssize_t read;
    struct dots *dots = NULL;
    char **pin_names = NULL;
    struct profile_pin **profile_pins = NULL;
    uint32_t num_pins = 0;
    uint32_t num_vecs = 0;
    char *real_path = NULL;

    if(util_fopen(dots_path, &fd, &fp, &file_size)){
        bye("error: failed to open file '%s'\n", dots_path);
    }

    if((real_path = realpath(dots_path, NULL)) == NULL){
        bye("invalid stim path '%s'\n", dots_path);
    }

    while((read = getline(&file_line, &len, fp)) != -1){
        file_line[strcspn(file_line,"\n")] = '\0';
        char *l = util_str_strip(file_line);

        if(l[0] == '#'){
            continue;
        }else if(strlen(l) == 0){
            continue;
        }else if(strstr(l, "Pins") == l){
            l = l + 4;
            l = util_str_strip(l);
            num_pins = util_str_split(l, ',', &pin_names);
            for(int i=0; i<num_pins; i++){
                char *s = strdup(util_str_strip(pin_names[i]));
                free(pin_names[i]);
                pin_names[i] = s;
            }
        }else if(strstr(l, "V") == l || strstr(l, "repeat") == l){
            num_vecs += 1;
        }else{
            bye("Invalid dots line: %s\n", l);
        }
    }

    if(num_pins == 0 || pin_names == NULL){
        bye("Failed to find valid pins in Pins line\n");
    } 

    if(num_vecs == 0){
        bye("Failed to find any vectors\n");
    }

    profile_pins = create_profile_pins(num_pins);
    for(int i=0; i<num_pins; i++){
        char *name = pin_names[i];
        if((profile_pins[i] = get_profile_pin_by_dest_pin_name(profile, -1, name)) == NULL){
            if((profile_pins[i] = get_profile_pin_by_net_alias(profile, -1, name)) == NULL){
                if((profile_pins[i] = get_profile_pin_by_net_name(profile, name)) == NULL){
                    bye("failed to get profile pin by name '%s'\n", name);
                }
            }
        }
    }

    if((dots = create_dots(num_vecs, profile_pins, num_pins)) == NULL){
        bye("failed to create dots\n");
    }

    fseek(fp, 0, SEEK_SET);

    while((read = getline(&file_line, &len, fp)) != -1){
        file_line[strcspn(file_line,"\n")] = '\0';
        char *l = util_str_strip(file_line);

        if(l[0] == '#'){
            continue;
        }else if(strlen(l) == 0){
            continue;
        }else if(strstr(l, "Pins") == l){
            continue;
        }else if(strstr(l, "V") == l){
            append_dots_vec_by_vec_str(dots, "1", l);
        }else if(strstr(l, "repeat") == l){
            char **vec_strs = NULL;
            if(util_str_split(l, ' ', &vec_strs) != 3){
                bye("invalid vec repeat line '%s'\n", l);
            }
            append_dots_vec_by_vec_str(dots, vec_strs[1], vec_strs[2]);
        }else{
            bye("Invalid dots line: %s\n", l);
        }
    }

    if(file_line){
        free(file_line);
    }

    fclose(fp);
    close(fd);

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
        die("error: pointer is NULL");
    }
    if(repeat == NULL){
        die("error: pointer is NULL");
    }
    if(vec_str == NULL){
        die("error: pointer is NULL");
    }

    if(dots->cur_appended_dots_vec_id >= dots->num_dots_vecs){
        die("error: failed to append dots vec exceeded \
            allocated limit of %i", dots->num_dots_vecs);
    }

    dots_vec = create_dots_vec(dots);
    dots_vec->repeat = strtoull(repeat, (char**)NULL, 10);
    dots_vec->vec_str = strdup(vec_str);
    dots->dots_vecs[dots->cur_appended_dots_vec_id++] = dots_vec;

    return;
}

/*
 * Appends given number of NOP vecs to dots.
 *
 */
void append_dots_vec_by_nop_vecs(struct dots *dots, uint32_t num_nop_vecs){
    uint32_t vec_len = 0;
    char *nop_vec_str = NULL;

    if(dots == NULL){
        die("pointer is NULL");
    }

    vec_len = dots->num_pins;

    if(vec_len == 0){
        die("failed to get vec_str len");
    }

    if((nop_vec_str = (char*)calloc(vec_len+1, sizeof(char))) == NULL){
        die("failed to malloc string");
    }
    for(int i=0; i<vec_len; i++){
        nop_vec_str[i] = 'X';
    }
    nop_vec_str[vec_len] = '\0';

    // re-allocate memory if needed
    if(dots->cur_appended_dots_vec_id >= dots->num_dots_vecs){
        dots->num_dots_vecs += num_nop_vecs;
        if((dots->dots_vecs = (struct dots_vec**)realloc(dots->dots_vecs, 
                dots->num_dots_vecs*sizeof(struct dots_vec*))) == NULL){
            die("failed to realloc memory");
        }
    }

    // always pad with NOPs
    for(int v=0; v<num_nop_vecs; v++){
        append_dots_vec_by_vec_str(dots, "1", nop_vec_str);
    }

    free(nop_vec_str);

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
        die("error: pointer is NULL");
    }

    if(dots_vec->is_expanded){
        die("error: dots_vec is already filled");
    }

    if(dots_vec->vec_str == NULL){
        die("error: pointer is NULL");
    }

    // len should only be config pins except data pins
    vec_str_len = (uint32_t)strlen(dots_vec->vec_str);

    // if dots represents a bitstream, check if vec length accounts for
    // the data pins
    if(data_subvecs != NULL){
        if((vec_str_len+PROFILE_NUM_DATA_PINS) != dots->num_pins){
            die("error: (vec_len + num_data_pins) %i != "
                "num_pins %i", (vec_str_len+PROFILE_NUM_DATA_PINS), dots->num_pins);
        }
    }

    if(dots_vec->num_subvecs != dots->num_pins){
        die("error: num subvecs for dots_vec %i != "
            "num_pins %i", dots_vec->num_subvecs, dots->num_pins);
    }

    if(dots_vec->subvecs != NULL){
        die("failed to expand dots_vec; subvecs is already allocated");
    }

    if((dots_vec->subvecs = (enum subvecs *)calloc(dots_vec->num_subvecs, sizeof(enum subvecs))) == NULL){
        die("error: failed to calloc dots_vecs' vecs");
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
            die("error: num_subvecs %i != num_pins %i", (vec_str_len+num_data_subvecs), dots->num_pins);
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
        die("pointer is NULL");
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
struct dots_vec *get_dots_vec_by_unrolled_id(struct dots *dots, uint64_t id){
    struct dots_vec *dots_vec = NULL;
    uint64_t start = 0;
    if(dots == NULL){
        die("error: pointer is NULL");
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
uint64_t get_num_unrolled_dots_vecs(struct dots *dots){
    uint64_t num_unrolled_vecs = 0;

    if(dots == NULL){
        die("pointer is NULL");
    }

    for(int i=0; i<dots->num_dots_vecs; i++){
        struct dots_vec *dots_vec = dots->dots_vecs[i];
        
        if(dots_vec == NULL){
            die("pointer is NULL");
        }

        uint32_t vec_str_len = (uint32_t)strlen(dots_vec->vec_str);
        for(int i=0; i<vec_str_len; i++){
            if(dots_vec->vec_str[i] == 'C'){
                dots_vec->has_clk = true;
            }
        }

        if(dots_vec->has_clk == true){
            num_unrolled_vecs += (dots_vec->repeat*2);
        }else{
            num_unrolled_vecs += dots_vec->repeat;
        }

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
        die("pointer is NULL");
    }

    if(num_pins == 0){
        die("cannot create a dots with zero pins");
    }

    if((dots = (struct dots*)malloc(sizeof(struct dots))) == NULL){
        die("error: failed to malloc struc");
    }
    dots->num_dots_vecs = num_dots_vecs;

    // always store compressed vecs 
    if((dots->dots_vecs = (struct dots_vec**)calloc(dots->num_dots_vecs, sizeof(struct dots_vec*))) == NULL){
        die("error: failed to calloc dots_vecs");
    }

    dots->num_pins = num_pins;

    if((dots->pins = (struct profile_pin **)calloc(dots->num_pins, sizeof(struct profile_pin*))) == NULL){
        die("error: failed to calloc profile pins");
    }

    for(uint32_t i=0; i<dots->num_pins; i++){
        dots->pins[i] = create_profile_pin_from_pin(pins[i]);
    }

    // when chunk is reading vecs from the dots, 
    // cur vec is stored here.
    dots->cur_a1_dots_vec_id = 0; 
    dots->cur_a2_dots_vec_id = 0; 

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
        die("pointer is NULL");
    }

    if((dots_vec = (struct dots_vec*)malloc(sizeof(struct dots_vec))) == NULL){
        die("error: failed to malloc struc");
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
        die("error: pointer is NULL");
    }
    
    for(uint32_t i=0; i<dots->cur_appended_dots_vec_id; i++){
        dots->dots_vecs[i] = free_dots_vec(dots->dots_vecs[i]);
    }
    free(dots->dots_vecs);
    dots->dots_vecs = NULL;
    dots->num_dots_vecs = 0;
    dots->cur_a1_dots_vec_id = 0; 
    dots->cur_a2_dots_vec_id = 0;
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
        die("error: pointer is NULL");
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
