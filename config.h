/*
 * Config helps with bitstream configuration of attached DUTs by using
 * information from profiles and pre-created vectors to generate a dots
 * object.
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "profile.h"
#include "dots.h"

#include <inttypes.h>

#define CONFIG_BIT_HEADER_SIZE (126)
#define CONFIG_VEC_CCLK_INDEX (0)


/*
 * Types
 *
 */

enum config_types {
    CONFIG_TYPE_NONE,
    CONFIG_TYPE_HEADER,
    CONFIG_TYPE_BODY,
    CONFIG_TYPE_FOOTER
};


/*
 * Bitstreams only provide D pin data. Config objects store info on how to
 * drive the config pins before, during and after the data.  
 *
 * num_dots_vecs : length of dots_vecs array
 *
 */
struct config {
    enum config_types type;
    struct dots *dots;
    uint32_t num_tags;
    enum profile_tags *tags;
};


/*
 * Prototypes
 *
 */

struct config *create_config(struct profile *profile, enum config_types type, uint32_t num_loop_vecs);
uint32_t get_config_num_vecs_by_type(enum config_types type);
uint64_t get_config_unrolled_num_vecs_by_type(enum config_types type);
uint32_t get_config_num_profile_pins_by_tag(enum profile_tags tag);
uint32_t get_config_num_profile_pins(void);
struct profile_pin **get_config_profile_pins(struct profile *profile, int32_t dut_id, uint32_t *found_num_pins);
struct config *free_config(struct config *config);

#ifdef __cplusplus
}
#endif
#endif
