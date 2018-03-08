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
struct profile_pin *create_profile_pin(struct profile_pin *copy_pin){
    struct profile_pin *profile_pin = NULL;
    if((profile_pin = (struct profile_pin*)malloc(sizeof(struct profile_pin))) == NULL){
        die("error: failed to malloc struct.\n");
    }
    profile_pin->pin_name = NULL;
    profile_pin->comp_name = NULL;
    profile_pin->net_name = NULL;
    profile_pin->net_alias = NULL;
    profile_pin->tag = PROFILE_TAG_NONE;
    profile_pin->tag_data = -1;
    profile_pin->dut_io_id = -1;

    if(copy_pin != NULL){
        profile_pin->pin_name = strdup(copy_pin->pin_name);
        profile_pin->comp_name = strdup(copy_pin->comp_name);
        profile_pin->net_name = strdup(copy_pin->net_name);
        profile_pin->net_alias = strdup(copy_pin->net_alias);
        profile_pin->tag = copy_pin->tag;
        profile_pin->tag_data = copy_pin->tag_data;
        profile_pin->dut_io_id = copy_pin->dut_io_id;
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
        die("error: failed to calloc struct.\n");
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
        die("error: failed to malloc struct.\n");
    }
    profile->path = NULL;
    profile->board_name = NULL;
    profile->description = NULL;
    profile->revision = 0;
    
    // allocate memory for all the pins. There should never be more than 400.
    profile->pins = create_profile_pins(400);
    profile->num_pins = 0;
    return profile;
}

/*
 * Deallocate the profile_pin struct and all internal members.
 *
 */
struct profile_pin *free_profile_pin(struct profile_pin *pin){
    if(pin == NULL){
        return NULL;
    }
    free(pin->pin_name);
    pin->pin_name = NULL;
    free(pin->comp_name);
    pin->comp_name = NULL;
    free(pin->net_name);
    pin->net_name = NULL;
    free(pin->net_alias);
    pin->net_alias = NULL;
    free(pin);
    return NULL;
}

struct profile_pin **free_profile_pins(struct profile_pin **pins, uint32_t num_pins){
    if(pins == NULL){
        return NULL;
    }
    for(int i=0; i<num_pins; i++){
        pins[i] = free_profile_pin(pins[i]);
    }
    free(pins);
    return NULL;
}

/*
 * Deallocate the profile struct and all internal members.
 *
 */
struct profile *free_profile(struct profile *p){
    if(p == NULL){
        return NULL;
    }
    free(p->path);
    p->path = NULL;
    free(p->board_name);
    p->board_name = NULL;
    free(p->description);
    p->description = NULL;
    p->pins = free_profile_pins(p->pins, p->num_pins);
    free(p);
    return NULL;
}

/*
 * Given a tag name string, return the tag enum.
 *
 */
enum profile_tags get_tag_by_name(char *name){
    if(name == NULL){
        die("error: failed to get tag by name, pointer is NULL\n");
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
void print_profile(struct profile *p){
    if(p == NULL){
        die("error: failed to print profile, pointer is NULL\n");
    }
    printf("path: %s\n", p->path);
    printf("board_name: %s\n", p->board_name);
    printf("description: %s\n", p->description);
    printf("revision: %i\n", p->revision);

    for(int i=0; i<p->num_pins; i++){
        struct profile_pin *pin = p->pins[i];
        if(pin == NULL){
            continue;
        }
        printf("  pin_name: %s\n", pin->pin_name);
        printf("  comp_name: %s\n", pin->comp_name);
        printf("  net_name: %s\n", pin->net_name);
        printf("  net_alias: %s\n", pin->net_alias);
        printf("  tag: %s\n", get_name_by_tag(pin->tag));
        printf("  tag_data: %i\n", pin->tag_data);
        printf("  dut_io_id: %i\n", pin->dut_io_id);
        printf("------------------------------------------------------\n");
    }
    return;
}

static inline int avl_comp(const void *key1, const void *key2){
    struct profile_pin *pin1 = (struct profile_pin *)key1;
    struct profile_pin *pin2 = (struct profile_pin *)key2;

    if(pin1 == NULL || pin2 == NULL){
        die("error: avl_comp failed, pointer is NULL\n");
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
    struct profile *profile;
    jsmn_parser json_parser;
    jsmntok_t t[2048*2];
    int32_t num_tokens;
    char *data;

    if(path == NULL){
        die("error: failed to get profile by path, pointer is NULL\n");
    }

    if(util_fopen(path, &fd, &fp, &file_size) != 0){
        die("fopen failed\n");
    }

    if((data = (char*)malloc(file_size)) == NULL){
        die("error: failed to malloc struct.\n");
    }

    fread(data, sizeof(uint8_t), file_size, fp);

    profile = create_profile();
    profile->path = realpath(path, NULL);

    jsmn_init(&json_parser);
	num_tokens = jsmn_parse(&json_parser, data, strlen(data), t, sizeof(t)/sizeof(t[0]));
    if(num_tokens < 0){
		printf("error: failed to parse json file: num_tokens (%d)\n", num_tokens);
		return NULL;
	}

	if(num_tokens < 1 || t[0].type != JSMN_OBJECT){
		printf("error: jsmn object expected\n");
		return NULL;
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
        }else if(util_jsmn_eq(data, &t[i], "pins") == 0){
			if(t[i+1].type != JSMN_ARRAY) {
				continue;
			}
            jsmntok_t *pins = &t[i+2];
            for(int j = 0; j < t[i+1].size; j++) {
                if (pins->type != JSMN_OBJECT) {
                    continue;
                }
                struct profile_pin *pin = create_profile_pin(NULL);
                for(int k=1; k < (pins->size*2)+1; k=k+2){
                    jsmntok_t *key = &pins[k+0];
                    jsmntok_t *v = &pins[k+1];
                    if(util_jsmn_eq(data, key, "pin_name") == 0){
                        pin->pin_name = strndup(data+v->start, v->end-v->start);
                    } else if (util_jsmn_eq(data, key, "comp_name") == 0){
                        pin->comp_name = strndup(data+v->start, v->end-v->start);
                    } else if (util_jsmn_eq(data, key, "net_name") == 0){
                        pin->net_name = strndup(data+v->start, v->end-v->start);
                    } else if (util_jsmn_eq(data, key, "net_alias") == 0){
                        pin->net_alias = strndup(data+v->start, v->end-v->start);
                    } else if (util_jsmn_eq(data, key, "tag_name") == 0){
                        pin->tag = get_tag_by_name(strndup(data+v->start, v->end-v->start));
                    } else if (util_jsmn_eq(data, key, "tag_data") == 0){
                        pin->tag_data = atoi(strndup(data+v->start, v->end-v->start));
                    } else if (util_jsmn_eq(data, key, "dut_io_id") == 0){
                        pin->dut_io_id = atoi(strndup(data+v->start, v->end-v->start));
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
            printf("error: dupliate profile pin found %s.\n", 
                profile->pins[i]->pin_name);
            return NULL;
        }
    }
    DestroyAVL(tree);

    return profile;
}

/*
 * Given an array of pins, sort the array by tag data, from LOW to HIGH. 
 * Note: if the tag_data is -1 it will error out.
 *
 */
struct profile_pin **sort_profile_pins_by_tag_data(struct profile_pin **pins, uint32_t num_pins){
    struct profile_pin **sorted_pins = create_profile_pins(num_pins); 

    // check if every pin has tag_data and make sure it's not greater than num_pins
    for(int i=0; i<num_pins; i++){
        if(pins[i]->tag_data < 0){
            die("error: failed to sort profile pins by "
                "tag, tag_data %i is invalid\n", pins[i]->tag_data);
        }
        if(pins[i]->tag_data >= num_pins){
            die("error: failed to sort profile pins by "
                "tag, tag_data %i > num_pins %i\n", pins[i]->tag_data, num_pins);
        }
    }

    // TODO: fix this and do proper sorting. Right now it just checks tag_data against index.
    for(int i=0; i<num_pins; i++){
        for(int j=0; j<num_pins; j++){
            if(pins[j]->tag_data == i){
                sorted_pins[i] = create_profile_pin(pins[j]); 
            }
        }
    }

    // free old profile pins and set the sorted, copied pins
    for(int i=0; i<num_pins; i++){
        pins[i] = free_profile_pin(pins[i]);
        pins[i] = create_profile_pin(sorted_pins[i]);
    }

    // free sorted_pins
    sorted_pins = free_profile_pins(sorted_pins, num_pins);
    return pins;
}

/*
 * Returns an array of found pins by given tag.
 *
 */
struct profile_pin **get_profile_pins_by_tag(struct profile *profile, 
        enum profile_tags tag, uint32_t *found_num_pins){
    struct profile_pin ** pins = NULL;
    (*found_num_pins) = 0;

    for(int i=0; i<profile->num_pins; i++){
        if(profile->pins[i]->tag == tag){
            (*found_num_pins)++;
        }
    }

    if((pins = (struct profile_pin**)calloc((*found_num_pins), sizeof(struct profile_pin*))) == NULL){
        die("error: failed to allocate pins by tag\n");
    }

    int j = 0;
    for(int i=0; i<profile->num_pins; i++){
        if(profile->pins[i]->tag == tag){
            if(j+1 > (*found_num_pins)){
                die("error: failed to get pins by tag, incorrect num_pins calculated\n");
            } else {
                pins[j] = profile->pins[i];
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
                die("error: failed to get pins by tag, did not find 1"
                    "%s pin, only %i\n", get_name_by_tag(tag), (*found_num_pins));
            }
            break;
        case PROFILE_TAG_DATA:
            if((*found_num_pins) != 32){
                die("error: failed to get pins by tag, did not find 32"
                    "%s pins, only %i\n", get_name_by_tag(tag), (*found_num_pins));
            }
            pins = sort_profile_pins_by_tag_data(pins, (*found_num_pins));
        case PROFILE_TAG_GPIO:
            break;
        default:
            break;
    }

    return pins;
}



