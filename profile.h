/*
 * Mezzanine Board Profiles
 *
 */

#ifndef PROFILE_H
#define PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <inttypes.h>
#include <stdbool.h>

#define PROFILE_NUM_DATA_PINS 32

/*
 * Types
 *
 */

enum profile_tags {
    PROFILE_TAG_NONE,
    PROFILE_TAG_CCLK,
    PROFILE_TAG_RESET_B,
    PROFILE_TAG_CSI_B,
    PROFILE_TAG_RDWR_B,
    PROFILE_TAG_PROGRAM_B,
    PROFILE_TAG_INIT_B,
    PROFILE_TAG_DONE,
    PROFILE_TAG_DATA,
    PROFILE_TAG_GPIO
};

struct profile_pin {
    char *pin_name;
    char *comp_name;
    char *net_name;
    char *net_alias;
    enum profile_tags tag;
    int32_t tag_data;
    int32_t dut_io_id;
};

/*
 * Parsed module netlists get turned into profiles. Each pin in the
 * profile represents a connection from each connector pin to one
 * fpga device-under-test. 
 *
 * TODO: support multiple duts
 *
 */
struct profile {
    char *path;
    char *board_name;
    char *description;
    uint32_t revision;
    struct profile_pin **pins;
    uint32_t num_pins;
};

/*
 * Prototypes
 *
 */
struct profile_pin *create_profile_pin(struct profile_pin *copy_pin);
struct profile_pin **create_profile_pins(uint32_t num_pins);
struct profile *create_profile();
struct profile_pin *free_profile_pin(struct profile_pin *pin);
struct profile_pin **free_profile_pins(struct profile_pin **pins, uint32_t num_pins);
struct profile *free_profile(struct profile *p);
enum profile_tags get_tag_by_name(char *name);
const char *get_name_by_tag(enum profile_tags tag);
void print_profile(struct profile *p);
struct profile *get_profile_by_path(const char *path);
struct profile_pin **sort_profile_pins_by_tag_data(struct profile_pin **pins, uint32_t num_pins);
struct profile_pin **get_profile_pins_by_tag(struct profile *profile, 
    enum profile_tags tag, uint32_t *found_num_pins);

#ifdef __cplusplus
}
#endif
#endif
