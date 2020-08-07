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
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <inttypes.h>
#include <stdbool.h>

#include "common.h"

// number if xilinx config data pins
#define PROFILE_NUM_DATA_PINS 32

// max pins from a1 and a2 connectors
#define PROFILE_MAX_PINS (400)

/*
 * Profile tags are classes that a pin can belong to. 
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

/*
 *
 * dut_id    : The device-under-test id. 
 * pin_name  : Pin name on the connector.
 * comp_name : Connector name in netlist.
 * net_name  : Name of the net from the connector pin
 *             to either a net-tie or the fpga pin(s).
 * net_alias : If connector pin is connected to a net-tie
 *             this is the net name before it.
 * tag       : A profile tag. 
 * tag_data  : If pin is tagged, this is the number extracted
 *             from the net_alias if applicable.
 * dut_io_id : If pin is part of the dut_io, this is the id
 *             extracted from the net name. The dut_io bus is
 *             from 0 to 399. If from A1 then 0 to 199. If from
 *             A2 then 200 to 399. 
 * num_dest_pin_names : number of dest pin names.
 * dest_pin_names : This is the vendor specific name of the pin on the part.
 *                  A pin can have many destination loads, if for example 
 *                  it's a 'shorted pin'. 
 *
 *
 *
 */
struct profile_pin {
    char *pin_name;
    char *comp_name;
    char *net_name;
    char *net_alias;
    enum profile_tags tag;
    int32_t tag_data;
    int32_t dut_io_id;
    uint32_t num_dests;
    uint32_t *dest_dut_ids;
    char **dest_pin_names;
};

/*
 * Parsed module netlists get turned into profiles. Each pin in the
 * profile represents a connection from each connector pin to one
 * fpga device-under-test. 
 *
 */
struct profile {
    char *path;
    char *board_name;
    char *description;
    uint32_t revision;
    struct profile_pin **pins;
    uint32_t num_pins;
    uint32_t num_duts;
};

/*
 * Prototypes
 *
 */
struct profile_pin *create_profile_pin(uint32_t num_dests);
struct profile_pin *create_profile_pin_from_pin(struct profile_pin *copy_pin);
struct profile_pin **create_profile_pins(uint32_t num_pins);
struct profile *create_profile(void);
struct profile_pin *free_profile_pin(struct profile_pin *pin);
struct profile_pin **free_profile_pins(
        struct profile_pin **pins, uint32_t num_pins);
struct profile *free_profile(struct profile *profile);
enum profile_tags get_tag_by_name(char *name);
const char *get_name_by_tag(enum profile_tags tag);
void print_profile(struct profile *profile);
void print_profile_pin(struct profile_pin *pin);
struct profile *get_profile_by_path(const char *path);
struct profile_pin **sort_profile_pins_by_tag_data(
        struct profile_pin **pins, uint32_t num_pins);
struct profile_pin **get_profile_pins_by_tag(struct profile *profile, 
        int32_t dut_id, enum profile_tags tag, uint32_t *found_num_pins);
struct profile_pin *get_profile_pin_by_dut_io_id(struct profile *profile, 
        uint32_t dut_io_id);
struct profile_pin *get_profile_pin_by_pin_name(struct profile *profile, 
        char *pin_name);
struct profile_pin *get_profile_pin_by_net_name(struct profile *profile, 
        char *net_name);
struct profile_pin *get_profile_pin_by_dest_pin_name(struct profile *profile, 
        int32_t dut_id, char *dest_pin_name);
struct profile_pin *get_profile_pin_by_net_alias(struct profile *profile, 
        int32_t dut_id, char *net_alias);
enum artix_selects get_artix_select_by_profile_pin(struct profile_pin *pin);
enum artix_selects get_artix_select_by_profile_pins(
        struct profile_pin **pins, uint32_t num_pins);


#ifdef __cplusplus
}
#endif
#endif
