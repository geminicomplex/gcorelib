/*
 * Handle mezzanine board json profiles.
 *
 */

// TODO: support multiple DUTs in profile and api

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "profile.h"
#include "util.h"

#include "lib/jsmn/jsmn.h"
#include "lib/avl/avl.h"

#include <limits.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>

/*
 * Allocate a new profile pin object.
 *
 */
struct profile_pin *create_profile_pin(uint32_t num_dests){
    struct profile_pin *profile_pin = NULL;

    if((profile_pin = (struct profile_pin*)malloc(sizeof(struct profile_pin))) == NULL){
        die("error: failed to malloc struct.");
    }

    profile_pin->pin_name = NULL;
    profile_pin->comp_name = NULL;
    profile_pin->net_name = NULL;
    profile_pin->net_alias = NULL;
    profile_pin->tag = PROFILE_TAG_NONE;
    profile_pin->tag_data = -1;
    profile_pin->dut_io_id = -1;
    profile_pin->num_dests = num_dests;
    profile_pin->dest_dut_ids = NULL;
    profile_pin->dest_pin_names = NULL;

    if(profile_pin->num_dests > 0){
        if((profile_pin->dest_dut_ids = (uint32_t *)calloc(profile_pin->num_dests, sizeof(uint32_t))) == NULL){
            die("error: failed to malloc struct.");
        }
        if((profile_pin->dest_pin_names = (char **)calloc(profile_pin->num_dests, sizeof(char*))) == NULL){
            die("error: failed to malloc struct.");
        }
    }

    return profile_pin;
}

/*
 * Allocate a new profile pin object.
 *
 */
struct profile_pin *create_profile_pin_from_pin(struct profile_pin *copy_pin){
    struct profile_pin *profile_pin = NULL;

    if(copy_pin == NULL){
        die("pointer is NULL");
    }

    if((profile_pin = create_profile_pin(copy_pin->num_dests)) == NULL){
        die("error: failed to malloc struct.");
    }
    
    profile_pin->pin_name = strdup(copy_pin->pin_name);
    profile_pin->comp_name = strdup(copy_pin->comp_name);
    profile_pin->net_name = strdup(copy_pin->net_name);
    profile_pin->net_alias = strdup(copy_pin->net_alias);
    profile_pin->tag = copy_pin->tag;
    profile_pin->tag_data = copy_pin->tag_data;
    profile_pin->dut_io_id = copy_pin->dut_io_id;
    profile_pin->num_dests = copy_pin->num_dests;

    
    for(uint32_t i=0; i<profile_pin->num_dests; i++){
        profile_pin->dest_dut_ids[i] = copy_pin->dest_dut_ids[i];
        profile_pin->dest_pin_names[i] = strdup(copy_pin->dest_pin_names[i]);
    }

    return profile_pin;
}


/*
 * Allocate an array of profile pin pointers given num_pins.
 *
 */
struct profile_pin **create_profile_pins(uint32_t num_pins){
    struct profile_pin **pins = NULL;
    if((pins = (struct profile_pin **)calloc(num_pins, sizeof(struct profile_pin*))) == NULL){
        die("error: failed to calloc struct.");
    }
    return pins;
}

/*
 * Allocate a new profile object.
 *
 */
struct profile *create_profile(){
    struct profile *profile;
    if((profile = (struct profile*)malloc(sizeof(struct profile))) == NULL){
        die("error: failed to malloc struct.");
    }
    profile->path = NULL;
    profile->board_name = NULL;
    profile->description = NULL;
    profile->revision = 0;
    
    // allocate memory for all the pins. There should never be more than 400.
    profile->pins = create_profile_pins(PROFILE_MAX_PINS);
    profile->num_pins = 0;
    profile->num_duts = 0;
    return profile;
}

struct profile_pin **free_profile_pins(struct profile_pin **pins, uint32_t num_pins){
    if(pins == NULL){
        die("pointer is NULL");
    }
    if(num_pins == 0){
        return NULL;
    }

    for(uint32_t i=0; i<num_pins; i++){
        pins[i] = free_profile_pin(pins[i]);
    }
    free(pins);
    return NULL;
}

/*
 * Deallocate the profile_pin struct and all internal members.
 *
 */
struct profile_pin *free_profile_pin(struct profile_pin *pin){
    if(pin == NULL){
        return NULL;
    }
    if(pin->pin_name != NULL){
        free(pin->pin_name);
        pin->pin_name = NULL;
    }
    if(pin->comp_name != NULL){
        free(pin->comp_name);
        pin->comp_name = NULL;
    }
    if(pin->net_name != NULL){
        free(pin->net_name);
        pin->net_name = NULL;
    }
    if(pin->net_alias != NULL){
        free(pin->net_alias);
        pin->net_alias = NULL;
    }
    pin->tag = PROFILE_TAG_NONE;
    pin->dut_io_id = -1;

    if(pin->dest_dut_ids != NULL){
        free(pin->dest_dut_ids);
    }
    pin->dest_dut_ids = 0;

    for(uint32_t i=0; i<pin->num_dests; i++){
        if(pin->dest_pin_names != NULL){
            free(pin->dest_pin_names[i]);
            pin->dest_pin_names[i] = NULL;
        }
    }

    if(pin->dest_pin_names != NULL){
        free(pin->dest_pin_names);
        pin->dest_pin_names = NULL;
    }

    pin->num_dests = 0;
    free(pin);
    return NULL;
}

/*
 * Deallocate the profile struct and all internal members.
 *
 */
struct profile *free_profile(struct profile *profile){
    if(profile == NULL){
        return NULL;
    }
    if(profile->path != NULL){
        free(profile->path);
        profile->path = NULL;
    }
    if(profile->board_name != NULL){
        free(profile->board_name);
        profile->board_name = NULL;
    }
    if(profile->description != NULL){
        free(profile->description);
        profile->description = NULL;
    }
    profile->revision = 0;
    if(profile->pins != NULL){
        for(uint32_t i=0; i<profile->num_pins; i++){
            profile->pins[i] = free_profile_pin(profile->pins[i]);
        }
        free(profile->pins);
        profile->pins = NULL;
    }
    profile->num_pins = 0;
    profile->num_duts = 0;
    free(profile);
    return NULL;
}

/*
 * Given a tag name string, return the tag enum.
 *
 */
enum profile_tags get_tag_by_name(char *name){
    if(name == NULL){
        die("error: failed to get tag by name, pointer is NULL");
    }
    enum profile_tags tag = PROFILE_TAG_NONE;
    if(strcmp(name, "NONE") == 0){
        tag = PROFILE_TAG_NONE;
    }else if (strcmp(name, "CCLK") == 0){
        tag = PROFILE_TAG_CCLK;
    }else if (strcmp(name, "RESET_B") == 0){
        tag = PROFILE_TAG_RESET_B;
    }else if (strcmp(name, "CSI_B") == 0){
        tag = PROFILE_TAG_CSI_B;
    }else if (strcmp(name, "RDWR_B") == 0){
        tag = PROFILE_TAG_RDWR_B;
    }else if (strcmp(name, "PROGRAM_B") == 0){
        tag = PROFILE_TAG_PROGRAM_B;
    }else if (strcmp(name, "INIT_B") == 0){
        tag = PROFILE_TAG_INIT_B;
    }else if (strcmp(name, "DONE") == 0){
        tag = PROFILE_TAG_DONE;
    }else if(strcmp(name, "DATA") == 0){
        tag = PROFILE_TAG_DATA;
    }else if (strcmp(name, "GPIO") == 0){
        tag = PROFILE_TAG_GPIO;
    }
    return tag;
}

/*
 * Given a tag enum, return the tag name string.
 *
 */
const char *get_name_by_tag(enum profile_tags tag){
    char *name = NULL;
    switch(tag){
        case PROFILE_TAG_NONE:
            name = strdup("NONE");
            break;
        case PROFILE_TAG_CCLK:
            name = strdup("CCLK");
            break;
        case PROFILE_TAG_RESET_B:
            name = strdup("RESET_B");
            break;
        case PROFILE_TAG_CSI_B:
            name = strdup("CSI_B");
            break;
        case PROFILE_TAG_RDWR_B:
            name = strdup("RDWR_B");
            break;
        case PROFILE_TAG_PROGRAM_B:
            name = strdup("PROGRAM_B");
            break;
        case PROFILE_TAG_INIT_B:
            name = strdup("INIT_B");
            break;
        case PROFILE_TAG_DONE:
            name = strdup("DONE");
            break;
        case PROFILE_TAG_DATA:
            name = strdup("DATA");
            break;
        case PROFILE_TAG_GPIO:
            name = strdup("GPIO");
            break;
        default:
            break;
    }
    return (const char*)name;
}

/*
 * Pretty prints the profile.
 *
 */
void print_profile(struct profile *profile){ 
    if(profile == NULL){
        die("error: failed to print profile, pointer is NULL");
    }
    printf("------------------------------------------------------\n");
    printf("path: %s\n", profile->path);
    printf("board_name: %s\n", profile->board_name);
    printf("description: %s\n", profile->description);
    printf("revision: %i\n", profile->revision);
    printf("num_duts: %i\n", profile->num_duts);
    printf("------------------------------------------------------\n");

    for(int i=0; i<profile->num_pins; i++){
        struct profile_pin *pin = profile->pins[i];
        if(pin == NULL){
            continue;
        }
        print_profile_pin(pin);
        printf("------------------------------------------------------\n");
    }
    return;
}

/*
 * Pretty print a profile pin.
 *
 */
void print_profile_pin(struct profile_pin *pin){
    uint32_t dest_pin_name_len = 0;
    char *dest_pin_names = NULL;
    char dest_dut_id_str[sizeof(uint32_t)*8+1];

    if(pin == NULL){
        die("pointer is NULL");
    }

    printf("  pin_name: %s\n", pin->pin_name);
    printf("  comp_name: %s\n", pin->comp_name);
    printf("  net_name: %s\n", pin->net_name);
    printf("  net_alias: %s\n", pin->net_alias);
    printf("  tag: %s\n", get_name_by_tag(pin->tag));
    printf("  tag_data: %i\n", pin->tag_data);
    printf("  dut_io_id: %i\n", pin->dut_io_id);
    printf("  num_dests: %i\n", pin->num_dests);

    // find how many chars to malloc
    dest_pin_name_len = 0;
    for(int j=0; j<pin->num_dests; j++){
        dest_pin_name_len += strlen(pin->dest_pin_names[j]);
        // add to for comma and space if not last
        if(j < pin->num_dests-1){
            dest_pin_name_len += 2;
        }
        snprintf(dest_dut_id_str, sizeof(uint32_t)*8+1, "dut_%i:", pin->dest_dut_ids[j]);
        dest_pin_name_len += strlen(dest_dut_id_str);
    }

    // +1 for terminating NULL character
    if((dest_pin_names = (char*)calloc((dest_pin_name_len+1), sizeof(char))) == NULL){
        die("malloc failed");
    }

    // cat the strings together
    for(int j=0, dest_pin_name_len=0; j<pin->num_dests; j++){
        snprintf(dest_dut_id_str, sizeof(uint32_t)*8+1, "dut_%i:", pin->dest_dut_ids[j]);
        strcat(dest_pin_names+dest_pin_name_len, dest_dut_id_str);
        dest_pin_name_len += strlen(dest_dut_id_str);
        
        strcat(dest_pin_names+dest_pin_name_len, pin->dest_pin_names[j]); 
        dest_pin_name_len += strlen(pin->dest_pin_names[j]);
        if(j < pin->num_dests-1){
            strcat(dest_pin_names+dest_pin_name_len, ", "); 
            dest_pin_name_len += 2;
        }
    }
    printf("  dest_pin_names: %s\n", dest_pin_names);
    free(dest_pin_names);
    return;
}

static inline int avl_comp(const void *key1, const void *key2){
    struct profile_pin *pin1 = (struct profile_pin *)key1;
    struct profile_pin *pin2 = (struct profile_pin *)key2;

    if(pin1 == NULL || pin2 == NULL){
        die("error: avl_comp failed, pointer is NULL");
    }
    return strcmp(pin1->pin_name, pin2->pin_name);
}

/*
 * Returns a profile given a board name.
 *
 */
struct profile *get_profile_by_path(const char *path){
    int fd;
    FILE *fp = NULL;
    off_t file_size;
    struct profile *profile = NULL;
    jsmn_parser json_parser;
    jsmntok_t t[2048*2];
    int32_t num_tokens;
    char *data;
    char *real_path = NULL;

    if(path == NULL){
        die("error: failed to get profile by path, pointer is NULL");
    }

    if(strlen(path) == 0){
        die("error: profile path is empty");
    }

    if(strcmp(util_get_file_ext_by_path(path), "json") != 0){
        die("profile path '%s' does not have a .json extension", path);
    }

    if((profile = create_profile()) == NULL){
        die("pointer is NULL");
    }

    if((real_path = realpath(path, NULL)) == NULL){
        die("invalid profile path '%s'", path);
    }

    profile->path = strdup(real_path);
    free(real_path);

    if(util_fopen(profile->path, &fd, &fp, &file_size) != 0){
        die("fopen failed");
    }

    if((data = (char*)calloc((size_t)file_size, sizeof(char))) == NULL){
        die("error: failed to malloc data.");
    }

    fread(data, sizeof(uint8_t), (size_t)file_size, fp);

    jsmn_init(&json_parser);
	num_tokens = jsmn_parse(&json_parser, data, strlen(data), t, sizeof(t)/sizeof(t[0]));
    if(num_tokens < 0){
		die("error: failed to parse json file: num_tokens (%d)", num_tokens);
	}

	if(num_tokens < 1 || t[0].type != JSMN_OBJECT){
		die("error: profile is not a valid json file");
	}

    for(int i=1; i<num_tokens; i++){
        if(util_jsmn_eq(data, &t[i], "board_name") == 0){
            profile->board_name = strndup(data+t[i+1].start, t[i+1].end-t[i+1].start); 
			i++;
        }else if(util_jsmn_eq(data, &t[i], "description") == 0){
            profile->description = strndup(data+t[i+1].start, t[i+1].end-t[i+1].start); 
			i++;
        }else if(util_jsmn_eq(data, &t[i], "revision") == 0){
            profile->revision = atoi(strndup(data+t[i+1].start, t[i+1].end-t[i+1].start)); 
			i++;
        }else if(util_jsmn_eq(data, &t[i], "num_duts") == 0){
            profile->num_duts = atoi(strndup(data+t[i+1].start, t[i+1].end-t[i+1].start)); 
            i++;
        }else if(util_jsmn_eq(data, &t[i], "pins") == 0){
			if(t[i+1].type != JSMN_ARRAY) {
				continue;
			}
            jsmntok_t *pins = &t[i+2];
            for(int j = 0; j < t[i+1].size; j++) {
                if (pins->type != JSMN_OBJECT) {
                    continue;
                }
                // don't allocate dests since we don't know yet
                struct profile_pin *pin = create_profile_pin(0);
                for(int k=1; k < (pins->size*2)+1; k=k+2){
                    jsmntok_t *key = &pins[k+0];
                    jsmntok_t *v = &pins[k+1];
                    if(util_jsmn_eq(data, key, "pin_name") == 0){
                        pin->pin_name = strndup(data+v->start, v->end-v->start);
                    }else if (util_jsmn_eq(data, key, "comp_name") == 0){
                        pin->comp_name = strndup(data+v->start, v->end-v->start);
                    }else if (util_jsmn_eq(data, key, "net_name") == 0){
                        pin->net_name = strndup(data+v->start, v->end-v->start);
                    }else if (util_jsmn_eq(data, key, "net_alias") == 0){
                        pin->net_alias = strndup(data+v->start, v->end-v->start);
                    }else if (util_jsmn_eq(data, key, "tag_name") == 0){
                        pin->tag = get_tag_by_name(strndup(data+v->start, v->end-v->start));
                    }else if (util_jsmn_eq(data, key, "tag_data") == 0){
                        pin->tag_data = atoi(strndup(data+v->start, v->end-v->start));
                    }else if (util_jsmn_eq(data, key, "dut_io_id") == 0){
                        pin->dut_io_id = atoi(strndup(data+v->start, v->end-v->start));
                    }else if (util_jsmn_eq(data, key, "dest_dut_ids") == 0){
                        char **dest_dut_ids = NULL;
                        pin->num_dests = util_str_split(strndup(data+v->start, v->end-v->start), 
                                ',', &(dest_dut_ids));
                        if((pin->dest_dut_ids = (uint32_t *)calloc(pin->num_dests, sizeof(uint32_t))) == NULL){
                            die("error: failed to malloc struct.");
                        }
                        for(uint32_t x=0; x<pin->num_dests; x++){
                            pin->dest_dut_ids[x] = atoi(dest_dut_ids[x]);
                            free(dest_dut_ids[x]);
                        }
                        free(dest_dut_ids);
                        // TODO: check if num_dests same as for pin_names
                    }else if (util_jsmn_eq(data, key, "dest_pin_names") == 0){
                        pin->num_dests = util_str_split(strndup(data+v->start, v->end-v->start), 
                                ',', &(pin->dest_pin_names));
                    }
                }
                profile->pins[profile->num_pins] = pin;
                profile->num_pins++;
                pins = &pins[(pins->size*2)+1];
			}
            i += t[i+1].size + 1;
        }
    }

    AVLTree *tree = CreateAVL(&avl_comp);

    // check for duplicate pins
    for(int i=0; i<profile->num_pins; i++){
        if(FindAVL(tree, (const void *)profile->pins[i]) == NULL){
            InsertAVL(tree, (const void *)profile->pins[i], NULL);
        } else {
            die("error: dupliate profile pin found %s.", profile->pins[i]->pin_name);
        }
    }
    DestroyAVL(tree);

    free(data);
    data = NULL;

    fclose(fp);
    close(fd);

    return profile;
}

/*
 * Given an array of pins, sort the array by tag data, from LOW to HIGH. 
 * Note: if the tag_data is -1 it will error out.
 *
 */
struct profile_pin **sort_profile_pins_by_tag_data(struct profile_pin **pins, uint32_t num_pins){
    struct profile_pin **sorted_pins = NULL;
    
    if(pins == NULL){
        die("pointer is NULL");
    }

    if(num_pins == 0){
        return pins;
    }

    // array to hold the sorted pointers
    if((sorted_pins = (struct profile_pin **)calloc(num_pins, sizeof(struct profile_pin*))) == NULL){
        die("error: failed to calloc struct.");
    }

    // check if every pin has tag_data and make sure it's not greater than num_pins
    for(uint32_t i=0; i<num_pins; i++){
        if(pins[i]->tag_data < 0){
            die("error: failed to sort profile pins by "
                "tag, tag_data %i is invalid", pins[i]->tag_data);
        }
        if(pins[i]->tag_data >= num_pins){
            die("error: failed to sort profile pins by "
                "tag, tag_data %i > num_pins %i", pins[i]->tag_data, num_pins);
        }
    }

    // TODO: fix this and do proper sorting. Right now it just checks tag_data against index.
    for(int i=0; i<num_pins; i++){
        for(int j=0; j<num_pins; j++){
            if(pins[j]->tag_data == i){
                sorted_pins[i] = pins[j]; 
            }
        }
    }

    // set sorted pins back to original
    for(int i=0; i<num_pins; i++){
        pins[i] = sorted_pins[i];
    }

    // free the array but not the pointers
    free(sorted_pins);
    return pins;
}

/*
 * Returns an array of found pins by given tag. You can optionally pass
 * a dut_id to filter by dut or pass -1 to filter by all pins.
 *
 */
struct profile_pin **get_profile_pins_by_tag(struct profile *profile, 
        int32_t dut_id, enum profile_tags tag, uint32_t *found_num_pins){
    struct profile_pin ** pins = NULL;
    (*found_num_pins) = 0;

    if(profile == NULL){
        die("pointer is NULL");
    }

    if(dut_id < -1){
        die("invalid dut_id given %i; less than -1", dut_id);
    }else if(dut_id >= 0 && (dut_id+1) > profile->num_duts){
        die("invalid dut_id given %i; greater than num duts %i", dut_id, profile->num_duts);
    }

    for(int i=0; i<profile->num_pins; i++){
        if(profile->pins[i]->tag == tag){
            if(dut_id >= 0){
                bool found = false;
                for(int j=0; j<profile->pins[i]->num_dests; j++){
                    if(profile->pins[i]->dest_dut_ids[j] == dut_id){
                        found = true;
                        break;
                    }
                }
                if(!found){
                    continue;
                }
            }
            (*found_num_pins)++;
        }
    }

    if((pins = create_profile_pins((*found_num_pins))) == NULL){
        die("error: failed to allocate pins by tag");
    }

    int j = 0;
    for(int i=0; i<profile->num_pins; i++){
        if(profile->pins[i]->tag == tag){
            if(j+1 > (*found_num_pins)){
                die("error: failed to get pins by tag, incorrect num_pins calculated");
            } else {
                if(dut_id >= 0){
                    bool found = false;
                    for(int k=0; k<profile->pins[i]->num_dests; k++){
                        if(profile->pins[i]->dest_dut_ids[k] == dut_id){
                            found = true;
                            break;
                        }
                    }
                    if(!found){
                        continue;
                    }
                }
                pins[j] = create_profile_pin_from_pin(profile->pins[i]);
                j++;
            }
        }
    }
    
    // check if correct number of pins are returned for each tag type.
    switch(tag){
        case PROFILE_TAG_NONE:
            break;
        case PROFILE_TAG_CCLK:
        case PROFILE_TAG_RESET_B:
        case PROFILE_TAG_CSI_B:
        case PROFILE_TAG_RDWR_B:
        case PROFILE_TAG_PROGRAM_B:
        case PROFILE_TAG_INIT_B:
        case PROFILE_TAG_DONE:
            if((*found_num_pins) != 1){
                die("error: failed to get pins by tag, did not find 1 "
                    "%s pin, only %i", get_name_by_tag(tag), (*found_num_pins));
            }
            break;
        case PROFILE_TAG_DATA:
            if((*found_num_pins) != 32){
                die("error: failed to get pins by tag, did not find 32"
                    "%s pins, only %i", get_name_by_tag(tag), (*found_num_pins));
            }
            pins = sort_profile_pins_by_tag_data(pins, (*found_num_pins));
        case PROFILE_TAG_GPIO:
            break;
        default:
            break;
    }

    return pins;
}

/*
 * Return a profile pin with a given pin name. Pin names are always unique.
 *
 */
struct profile_pin *get_profile_pin_by_pin_name(struct profile *profile, char *pin_name){
    struct profile_pin *found_pin = NULL;

    if(profile == NULL){
        die("pointer is NULL");
    }

    if(pin_name == NULL){
        die("pointer is NULL");
    }

    for(int i=0; i<profile->num_pins; i++){
        if(strcmp(profile->pins[i]->pin_name, pin_name) == 0){
            if(found_pin != NULL){
                die("found multiple pins for pin_name '%s'", pin_name);
            }else{
                found_pin = create_profile_pin_from_pin(profile->pins[i]);
            }
        }
    }
    return found_pin;
}

/*
 * Return a profile pin that connects to a given vendor specific dest pin name.
 * Note that one profile pin can connect to many dest pin names. For example,
 * if we supported 'shorted pin' groups. You can optionally pass a dut_id to 
 * filter by dut or pass -1 to filter by all pins.
 *
 */
struct profile_pin *get_profile_pin_by_dest_pin_name(struct profile *profile, int32_t dut_id, char *dest_pin_name){
    struct profile_pin *found_pin = NULL;

    if(profile == NULL){
        die("pointer is NULL");
    }

    if(dest_pin_name == NULL){
        die("pointer is NULL");
    }

    if(dut_id < -1){
        die("invalid dut_id given %i; less than -1", dut_id);
    }else if(dut_id >= 0 && (dut_id+1) > profile->num_duts){
        die("invalid dut_id given %i; greater than num duts %i", dut_id, profile->num_duts);
    }

    for(int i=0; i<profile->num_pins; i++){
        for(int j=0; j<profile->pins[i]->num_dests; j++){
            if(dut_id >= 0 && profile->pins[i]->dest_dut_ids[j] != dut_id){
                continue;
            }
            if(strcmp(dest_pin_name, profile->pins[i]->dest_pin_names[j]) == 0){
                if(found_pin != NULL){
                    die("dest_pin_name '%s' is driven by multiple pins for profile '%s'", 
                            dest_pin_name, profile->path);
                }else{
                    found_pin = create_profile_pin_from_pin(profile->pins[i]);
                }
            }
        }
    }
    return found_pin;
}
/*
 * Get a profile pin by the net alias name. Note that the net alias is unique
 * for a given dut. The net alias name can be used across duts if you have
 * more than one, but again, never more than once within a dut. Pass a -1
 * to search all duts, however, it will error out if it finds more than one
 * dut with the name. 
 *
 */
struct profile_pin *get_profile_pin_by_net_alias(struct profile *profile, int32_t dut_id, char *net_alias){
    struct profile_pin *found_pin = NULL;

    if(profile == NULL){
        die("pointer is NULL");
    }

    if(net_alias == NULL){
        die("pointer is NULL");
    }

    if(dut_id < -1){
        die("invalid dut_id given %i; less than -1", dut_id);
    }else if(dut_id >= 0 && (dut_id+1) > profile->num_duts){
        die("invalid dut_id given %i; greater than num duts %i", dut_id, profile->num_duts);
    }

    for(int i=0; i<profile->num_pins; i++){
        if(dut_id >= 0 && profile->pins[i]->dut_io_id != dut_id){
            continue;
        }
        if(strcmp(net_alias, profile->pins[i]->net_alias) == 0){
            if(found_pin != NULL){
                if(dut_id == -1){
                    die("Multiple net_alias '%s' found when searching all duts for profile '%s'.\
                            Pass a dut_id to filter by dut.", 
                            net_alias, profile->path);
                }else{
                    die("multiple net_alias '%s' found for dut id '%d' for profile '%s'", 
                        net_alias, dut_id, profile->path);
                }
            }else{
                found_pin = create_profile_pin_from_pin(profile->pins[i]);
            }
        }
    }
    return found_pin;
}

/*
 * Profile pins can be part of the dut_io bus or other miscellaneous pin.
 * This returns if the dut_io_id belongs to none, a1 or a2.
 *
 */
enum artix_selects get_artix_select_by_profile_pin(struct profile_pin *pin){
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    if(pin == NULL){
        die("pointer is NULL");
    }

    if(pin->dut_io_id == -1){
        artix_select = ARTIX_SELECT_NONE;
    }else if(pin->dut_io_id >= 0 && pin->dut_io_id < 200){
        artix_select = ARTIX_SELECT_A1;
    }else if(pin->dut_io_id >= 200 && pin->dut_io_id < 400){
        artix_select = ARTIX_SELECT_A2; 
    }else{
        die("invalid dut_io_id '%i' found", pin->dut_io_id);
    }

    return artix_select;
}

enum artix_selects get_artix_select_by_profile_pins(struct profile_pin **pins, uint32_t num_pins){
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    if(pins == NULL){
        die("pointer is NULL");
    }

    for(uint32_t i=0; i<num_pins; i++){
        if(get_artix_select_by_profile_pin(pins[i]) == ARTIX_SELECT_A1){
            if(artix_select == ARTIX_SELECT_A2){
                artix_select = ARTIX_SELECT_BOTH;
                break;
            }else {
                artix_select = ARTIX_SELECT_A1;
            }
        }else if(get_artix_select_by_profile_pin(pins[i]) == ARTIX_SELECT_A2){
            if(artix_select == ARTIX_SELECT_A1){
                artix_select = ARTIX_SELECT_BOTH;
                break;
            }else {
                artix_select = ARTIX_SELECT_A2;
            }
        }
    }

    return artix_select;
}



