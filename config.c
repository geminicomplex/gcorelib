/*
 * bitstream configuration
 *
 */

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "config.h"
#include "profile.h"
#include "util.h"
#include "dots.h"

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
 * The following order will be used when getting pins
 * from the profile.
 *
 */
const enum profile_tags config_tags[] = {
    PROFILE_TAG_CCLK,
    PROFILE_TAG_RESET_B,
    PROFILE_TAG_CSI_B,
    PROFILE_TAG_RDWR_B,
    PROFILE_TAG_PROGRAM_B,
    PROFILE_TAG_INIT_B,
    PROFILE_TAG_DONE,
    PROFILE_TAG_DATA
};

/*
 * Vectors for just the config pins, don't need to include D pin data,
 * since that will get passed at run time and injected into the vector.
 *
 * format: repeat, CCLK, RESET_B, CSI_B, RDWR_B, PROGRAM_B, INIT_B, DONE
 *
 */
const char *config_header[] = {
    "5",      "011111X", // initial state
    "5",      "001111X", // reset board
    "5",      "011111X", // de-assert reset_b
    "30",     "011100X", // assert prog_b and init_b
    "50000",  "C11110X", // de-assert prog_b, keep init_b asserted
    "1",      "C11111L", // de-assert init_b
    "30",     "C11011L", // assert rdwr_b 
    "1",      "C10011L"  // turn cclk on 
}; //100,109

// Repeat value is just dummy data. Will be set to number of words found
// in bitstream, which corresponds to number of vectors.
const char *config_body[] = {
    "1", "C1001HX"       // assert csi_b and rdwr_b while writing config
                         // init_b will assert if crc error is encountered
}; // 167,826 or 83913

// warning: the last vector is used to pad a chunk. Make sure it doesn't have
// a clock nor a repeat greater than one.
const char *config_footer[] = {
    "33",     "C1101HX", // turn cclk off, de-assert csi_b 
    "50000",  "C1101HX", // wait for done to assert 
    "5",      "C1101HH", // check if done went high
    "1",      "011111X"  // de-assert rdwr_b
}; // 50043

/*
 * Return num of vecs which is just the array divided by two since
 * we store the repeat and the vector string separately.
 *
 *
 */
uint32_t get_config_num_vecs_by_type(enum config_types type){
    uint32_t num_vecs = 0;
    if(type == CONFIG_TYPE_NONE){
        die("error: no config type given\n");
    }else if(type == CONFIG_TYPE_HEADER){
        num_vecs = (sizeof(config_header)/sizeof(config_header[0]))/2; 
    }else if(type == CONFIG_TYPE_BODY){
        num_vecs = (sizeof(config_body)/sizeof(config_body[0]))/2; 
    }else if(type == CONFIG_TYPE_FOOTER){
        num_vecs = (sizeof(config_footer)/sizeof(config_footer[0]))/2; 
    }
    return num_vecs;
}

/*
 * Add up all the repeat values for each vector, double the value if the 
 * clock is enabled to get the actual number of vectors.
 *
 */
uint32_t get_config_unrolled_num_vecs_by_type(enum config_types type){
    const char **vec_data = NULL;
    uint32_t unrolled_num_vecs = 0;

    if(type == CONFIG_TYPE_NONE){
        die("error: no config type given\n");
    }else if(type == CONFIG_TYPE_HEADER){
        vec_data = config_header;
    }else if(type == CONFIG_TYPE_BODY){
        vec_data = config_body;
    }else if(type == CONFIG_TYPE_FOOTER){
        vec_data = config_footer;
    }

    if(vec_data == NULL){
        die("error: vec_data is NULL\n");
    }

    uint32_t num_vecs = get_config_num_vecs_by_type(type);

    for(int i=0; i<(num_vecs*2); i=i+2){
        uint32_t repeat = atoi(vec_data[i+0]);
        if(vec_data[i+1][CONFIG_VEC_CCLK_INDEX] == 'C'){
            repeat *= 2;
        }
        unrolled_num_vecs += repeat;
    }
    return unrolled_num_vecs;
}

/*
 * Returns the number of pins per tag, given that it's a config
 * tag otherwise it would be unknown.
 *
 */
uint32_t get_config_num_profile_pins_by_tag(enum profile_tags tag){
    uint32_t num_pins = 0;
    uint32_t num_tags = sizeof(config_tags)/sizeof(config_tags[0]);

    bool found = false;
    for(int i=0; i<num_tags; i++){
        if(tag == config_tags[i]){
            found = true;
            break;
        }
    }

    if(found == false){
        die("error: given tag '%s', is not a config tag\n", get_name_by_tag(tag));
    }
    
    if(tag == PROFILE_TAG_DATA){
        return 32;
    }else{
        return 1;
    }
    return num_pins;
}

/*
 * Return total number of config profile pins.
 *
 */
uint32_t get_config_num_profile_pins(){
    uint32_t num_pins = 0;
    uint32_t num_tags = sizeof(config_tags)/sizeof(config_tags[0]);

    for(int i=0; i<num_tags; i++){
        num_pins += get_config_num_profile_pins_by_tag(config_tags[i]);
    }

    return num_pins;
}

/*
 * Return the PROFILE_NUM_CONFIG_PINS config pins needed to configure the DUT in the order of the 
 * config_tags array. You can optionally pass a dut_id to filter by dut or pass -1 to return all pins.
 *
 */
struct profile_pin **get_config_profile_pins(struct profile *profile, int32_t dut_id, uint32_t *found_num_pins){
    // we need 39 pins, cclk, reset_b, csi_b, rdwr_b, program_b, init_b, done, 32 data,
    uint32_t num_config_pins = 0;
    struct profile_pin **config_pins = NULL;
    uint32_t num_pins = 0;
    uint32_t num_found_pins = 0;
    struct profile_pin **pins = NULL;
    uint32_t num_tags = sizeof(config_tags)/sizeof(config_tags[0]);

    if(profile == NULL){
        die("pointer is NULL\n");
    }

    if(found_num_pins == NULL){
        die("pointer is NULL\n");
    }

    num_config_pins = get_config_num_profile_pins();
    config_pins = create_profile_pins(num_config_pins);

    if(dut_id < -1){
        die("invalid dut_id given %i; less than -1\n", dut_id);
    }else if(dut_id >= 0 && (dut_id+1) > profile->num_duts){
        die("invalid dut_id given %i; greater than num duts %i\n", dut_id, profile->num_duts);
    }

    // reset found pins
    (*found_num_pins) = 0;

    // get pins for each tag type, and copy them to config_pins
    for(int i=0; i<num_tags; i++){
        num_pins = 0;
        pins = NULL;
        if((pins = get_profile_pins_by_tag(profile, dut_id, config_tags[i], &num_pins)) == NULL){
            die("error: pointer is NULL\n");
        }
        for(int j=0; j<num_pins; j++){
            if((*found_num_pins) < num_config_pins){
                config_pins[(*found_num_pins)++] = create_profile_pin(pins[j], pins[j]->num_dests);
            }else{
                die("error: failed to get config pins, length exceeded.\n");
            }
        }
        num_found_pins += num_pins;
        pins = free_profile_pins(pins, num_pins);
    }

    if(num_found_pins != num_config_pins){
        die("error: failed to get config profile pins, "
            "got %i but expected %i\n", num_found_pins, num_config_pins);
    }

    return config_pins;
}

/*
 * Allocate a new config object. The num loop vecs parameter will loop the
 * entire vec set given times. Need this since the body content is dynamic
 * and we'll use the body vecs as a template to loop and inject the data
 * into the subvecs. Should be set to 1 if not body so it'll loop the 
 * set only once.
 *
 */
struct config *create_config(struct profile *profile, enum config_types type, uint32_t num_loop_vecs, uint32_t num_padding_vecs){
    struct config *config = NULL;
    uint32_t num_pins = 0;
    struct profile_pin **pins = NULL;

    if(type == CONFIG_TYPE_NONE){
        die("error: no config type given\n");
    }

    if(num_loop_vecs <= 0){
        die("error: must loop at least once\n");
    }else if(type != CONFIG_TYPE_BODY && num_loop_vecs != 1){
        die("error: num loop vecs must be one for non-body type config");
    }

    if((config = (struct config*)malloc(sizeof(struct config))) == NULL){
        die("error: failed to malloc struct\n");
    }

    config->type = type;

    uint32_t num_dots_vecs = get_config_num_vecs_by_type(config->type);

    if(type == CONFIG_TYPE_BODY && num_dots_vecs != 1){
        die("error: for config type body, only supports one vector in the template\n");
    }

    // TODO: dut_id = -1 filters by all duts. Pass the correct dut_id when supported multiple-duts.
    int32_t dut_id = -1;
    if((pins = get_config_profile_pins(profile, dut_id, &num_pins)) == NULL){
        die("error: failed to get profile config pins\n");
    }
    config->dots = create_dots((num_loop_vecs*num_dots_vecs)+num_padding_vecs, pins, num_pins);

    // save order of config pins, so stim can iterate through correct
    // order when accessing it's pins
    config->num_tags = sizeof(config_tags)/sizeof(config_tags[0]);
    config->tags = (enum profile_tags*)config_tags;

    const char **vec_data = NULL;
    if(config->type == CONFIG_TYPE_NONE){
        die("error: no config type given\n");
    }else if(config->type == CONFIG_TYPE_HEADER){
        vec_data = config_header;
    }else if(config->type == CONFIG_TYPE_BODY){
        vec_data = config_body;
    }else if(config->type == CONFIG_TYPE_FOOTER){
        vec_data = config_footer;
    }

    if(vec_data == NULL){
        die("error: vec_data is NULL\n");
    }

    for(int v=0; v<num_loop_vecs; v++){
        for(int i=0; i<(num_dots_vecs*2); i=i+2){
            append_dots_vec_by_vec_str(config->dots, vec_data[i+0], vec_data[i+1]);
        }
    }

    // TODO: do a nop instead of the last vector

    // always pad with the last vector
    for(int v=0; v<num_padding_vecs; v++){
        for(int i=((num_dots_vecs*2)-2); i<(num_dots_vecs*2); i=i+2){
            append_dots_vec_by_vec_str(config->dots, vec_data[i+0], vec_data[i+1]);
        }
    }

    return config;
}

/*
 * De-allocates config vecs struct and internal members.
 *
 */
struct config *free_config(struct config *config){
    if(config == NULL){
        die("error: pointer is NULL\n");
    }
    config->dots = free_dots(config->dots);
    config->tags = NULL;
    free(config);
    return NULL;
}





