/*
 * parse dots and rbt/bit files into native structs
 *
 */

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "stim.h"
#include "util.h"
#include "profile.h"
#include "config.h"
#include "lib/avl/avl.h"

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

// Mac OS X / Darwin features
#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#else
#include <byteswap.h>
#endif

#define BUFFER_LENGTH (4096)

/*
 * Given a vector , pack the subvec for the given dut_io_id.
 *
 */
void pack_subvecs_by_dut_io_id(uint8_t *packed_subvecs, uint32_t dut_io_id, enum subvecs subvec){
    if(packed_subvecs == NULL){
        die("error: pointer is null\n");
    }
    if(dut_io_id >= (DUT_TOTAL_NUM_PINS-1)){
        die("error: failed to pac vec, dut_io_id %i out of range\n", dut_io_id);
    }

    // clamp the id from 0 to 199 since we're only writing to one
    // dut at a time and so packed_subvecs will always be len of 200
    dut_io_id = dut_io_id % DUT_NUM_PINS;

    uint32_t subvec_id = dut_io_id/2;
    uint8_t packed_subvec = packed_subvecs[subvec_id];

    // note: because of the way we pack 64bit words into the 1024 bit
    // fifo and because the bus is [1023:0] in dutcore, we need to 
    // pack high dut_io to low dut_io from msb to lsb

    // clear and load high nibble
    if((dut_io_id % 2) == 1){
        packed_subvec = ((subvec << 4) | 0x0f) & (packed_subvec | 0xf0);
    // clear and load low nibble
    } else if((dut_io_id % 2) == 0){
        packed_subvec = ((subvec << 0) | 0xf0) & (packed_subvec | 0x0f);
    }

    packed_subvecs[subvec_id] = packed_subvec;
    
    return;
}

/*
 * Given a vector, packs the opcode and it's operand in the remaining space.
 *
 * TODO: operand is 27 bytes long, but we're only using 32bits of it. Support the full size.
 *
 */
void pack_subvecs_with_opcode_and_operand(uint8_t *packed_subvecs, enum vec_opcode opcode, uint32_t operand){
    if(packed_subvecs == NULL){
        die("error: pointer is null\n");
    }

    // opcode is at 127
    uint8_t opcode_index = STIM_VEC_SIZE-1;
    packed_subvecs[opcode_index] = opcode;

    // operand is at 100 stored as little endian
    uint8_t operand_index = 100;

    for(int i=100; i<=126; i++){
        packed_subvecs[i] = 0x00;
    }
    packed_subvecs[operand_index+0] = *((uint8_t*)(&operand)+0);
    packed_subvecs[operand_index+1] = *((uint8_t*)(&operand)+1);
    packed_subvecs[operand_index+2] = *((uint8_t*)(&operand)+2);
    packed_subvecs[operand_index+3] = *((uint8_t*)(&operand)+3);
    //
    //packed_subvecs[operand_index] = 1;
    //
    //packed_subvecs[100] = operand;
    return;
}

/*
 * Given a vector, return the subvec from the packed_subvecs given 
 * a pin id.
 *
 */
enum subvecs get_subvec_by_pin_id(struct vec *vec, uint32_t pin_id){
    if(vec == NULL){
        die("error: failed to pack vec by pin, pointer is NULL\n");
    }
    if((pin_id+1) >= DUT_TOTAL_NUM_PINS){
        die("error: failed to pack vec by pin, pin_id+1 >= 400\n");
    }

    enum subvecs subvec = DUT_SUBVEC_NONE; 

    uint32_t subvec_id = pin_id/2;
    uint8_t packed_subvec = vec->packed_subvecs[subvec_id];

    // get high nibble
    if((pin_id % 2) == 0){
        packed_subvec = ((packed_subvec & 0xf0) >> 4);
    // get low nibble
    } else if((pin_id % 2) == 1){
        packed_subvec = ((packed_subvec & 0x0f) >> 0);
    }

    if(packed_subvec == (uint8_t)DUT_SUBVEC_0){
        subvec = DUT_SUBVEC_0;
    }else if(packed_subvec == (uint8_t)DUT_SUBVEC_1){
        subvec = DUT_SUBVEC_1;
    }else if(packed_subvec == (uint8_t)DUT_SUBVEC_X){
        subvec = DUT_SUBVEC_X;
    }else if(packed_subvec == (uint8_t)DUT_SUBVEC_H){
        subvec = DUT_SUBVEC_H;
    }else if(packed_subvec == (uint8_t)DUT_SUBVEC_L){
        subvec = DUT_SUBVEC_L;
    }else if(packed_subvec == (uint8_t)DUT_SUBVEC_C){
        subvec = DUT_SUBVEC_C;
    }else{
        die("error: failed to parse uint8_t subvec %i\n", subvec);
    }

    return subvec;
}

/*
 * Given a 32 bit word, iterate through the bits and return 
 * an array of 0 or 1 subvecs from [0:31]. 
 *
 * A bitstream word is stored as [31:0] after it's read of disk
 * and it's endianess is swapped. It's passed here as [31:0]
 * 
 * on disk (little endian): 66_55_99_AA
 * after mem read: AA_99_55_66 => D pins [31:0]
 *
 * Per v7 config datasheet, all the bits in each byte must be swapped.
 *
 * after swap: 55_99_AA_66
 *
 */
enum subvecs *convert_bitstream_word_to_subvecs(uint32_t *word, 
        uint32_t *num_subvecs){
    enum subvecs *subvecs = NULL;
    uint8_t num_bits = (sizeof(uint32_t)*8);
    
    if(word == NULL){
        die("pointer is NULL\n");
    }

    if((subvecs = (enum subvecs *)calloc(num_bits, sizeof(enum subvecs))) == NULL){
        die("error: failed to calloc subvecs\n");
    }

    int byte = 0;
    for(int i=0; i<num_bits; i++){
        if(i % 8 == 0){
            byte++;
        }
        // swap the bits in each byte
        int bit = ((8*byte)-1)-(i%8);
        if((((*word) >> i) & 0x00000001) == 1){
            subvecs[bit] = DUT_SUBVEC_1;
        } else {
            subvecs[bit] = DUT_SUBVEC_0;
        }
    }

    (*num_subvecs) = num_bits;

    return subvecs;
}

/*
 * Following three functions will read either 8, 16 or 32 bits
 * and increment the map byte offset. Note that regardless
 * if the machine is 32bit or 64bit, the data is stored little
 * endian on disk. After accessing the address, type-casting,
 * and de-referencing, it will be swapped.  
 *
 */
static inline uint8_t read_map_8(struct stim *stim){
    if(stim == NULL){
        die("pointer is NULL\n");
    }
    uint8_t *byte = (uint8_t*)&(stim->map)[stim->cur_map_byte];
    if(byte == NULL){
        die("map byte is NULL\n");
    }
    stim->cur_map_byte += 1;
    return *byte;
}

static inline uint16_t read_map_16(struct stim *stim, bool swap_endian){
    if(stim == NULL){
        die("pointer is NULL\n");
    }
    uint16_t *halfword = (uint16_t*)&(stim->map)[stim->cur_map_byte];
    if(halfword == NULL){
        die("map halfword is NULL\n");
    }
    stim->cur_map_byte += 2;
    if(swap_endian){
        return bswap_16(*halfword);
    }
    return *halfword;
}

static inline uint32_t read_map_32(struct stim *stim, bool swap_endian){
    if(stim == NULL){
        die("pointer is NULL\n");
    }
    uint32_t *word = (uint32_t*)&(stim->map)[stim->cur_map_byte];
    if(word == NULL){
        die("map word is NULL\n");
    }
    stim->cur_map_byte += 4;
    if(swap_endian){
        return bswap_32(*word);
    }
    return *word;
}

/*
 * The mmaped file is just raw bytes. Return content aware words based on the
 * file's data type, taking into account the endianess. Always return with the
 * ordering from D31 to D00.
 *
 */
uint32_t stim_get_next_bitstream_word(struct stim *stim){
    uint32_t word = 0;
    char buffer[BUFFER_LENGTH];
    switch(stim->type){
        case STIM_TYPE_RBT:
            int32_t c = 0;
            uint32_t count = 0;
            while(stim->cur_map_byte < stim->file_size){
                if(stim->map[stim->cur_map_byte+count] == '\n'){
                    if(count > BUFFER_LENGTH){
                        die("buffer overflow; gross\n");
                    }
                    memset(buffer, '\0', BUFFER_LENGTH);
                    memcpy(buffer, (stim->map+stim->cur_map_byte), count);
                    if(count != 32){
                        die("rbt bitstream word is not 32 bits\n");
                    }
                    c = count;
                    word = 0;
                    while((--c) >= 0){
                        if(buffer[c] == '0'){
                            continue;
                        }else if(buffer[c] == '1'){
                            word += (1<<((count-1)-c));
                        }else{
                            die("invalid rbt bitstream at byte %i\n",
                                stim->cur_map_byte+c);
                        }
                    }
                    stim->cur_map_byte += (count+1);
                    count = 0;
                    break;
                }else{
                    count += 1;
                }
            }
                
            break;
        case STIM_TYPE_BIN:
        case STIM_TYPE_BIT:
            if(stim->cur_map_byte > (stim->file_size-1)){
                return NULL;
            } else {
                if(stim->is_little_endian){
                    word = read_map_32(stim, false);
                }else{
                    word = read_map_32(stim, true);
                }
            }
            break;
        case STIM_TYPE_DOTS:
        case STIM_TYPE_RAW:
        case STIM_TYPE_NONE:
        default:
            die("error: file is not a bitstream\n");
            break;
    }

    return word;
}

/*
 * Malloc and initialize new vec struct and internal members.
 *
 */
struct vec *create_vec(){
    struct vec *vec = NULL;

    if((vec = (struct vec*)malloc(sizeof(struct vec))) == NULL){
        die("error: failed to malloc vec struct\n.");
    }

    if((STIM_VEC_SIZE % 2) != 0){
        die("error: stim vec size %d must be a multiple of 2\n", STIM_VEC_SIZE);
    }
   
    if((vec->packed_subvecs = (uint8_t*)calloc(STIM_VEC_SIZE, sizeof(uint8_t))) == NULL){
        die("error: failed to calloc vec's subvecs uint8_t\n");
    }

    for(int i=0; i<STIM_VEC_SIZE; i++){
        vec->packed_subvecs[i] = 0xff;
    }
    return vec;
}

/* 
 * Create new vec_chunk object. Because we can only store so many vectors 
 * in memory at a time, we split them into chunks and load them as needed
 * from an mmap source.
 *
 */
struct vec_chunk *create_vec_chunk(uint8_t vec_chunk_id, uint32_t num_vecs){
    struct vec_chunk *chunk = NULL;

    if(num_vecs == 0){
        die("error: num_vecs == 0, failed to malloc vec_chunk struct\n");
    }

    if((chunk = (struct vec_chunk*)malloc(sizeof(struct vec_chunk))) == NULL){
        die("error: failed to malloc vec_chunk struct\n.");
    }

    chunk->id = vec_chunk_id;
    chunk->is_loaded = false;
    chunk->num_vecs = num_vecs;

    // don't malloc until we load the chunk to save on memory
    chunk->cur_vec_id = 0;

    chunk->vec_data = NULL;
    chunk->vec_data_size = 0;
    
    // number of bytes for loaded vecs
    chunk->size = 0;

    return chunk;
}

/*
 * Allocate a new stim object. Note that it is not initialized yet.
 *
 */
struct stim *create_stim(){
    struct stim *stim = NULL;

    if((stim = (struct stim*)malloc(sizeof(struct stim))) == NULL){
        die("error: failed to malloc stim struct\n.");
    }

    if((stim->pins = (struct profile_pin **)calloc(DUT_TOTAL_NUM_PINS, sizeof(struct profile_pin*))) == NULL){
        die("error: failed to calloc stim vecs\n");
    }

    for(int i=0; i<DUT_TOTAL_NUM_PINS; i++){
        stim->pins_active[i] = false;
        stim->pins[i] = NULL;
    }

    stim->type = STIM_TYPE_NONE;
    stim->num_pins = 0;
    stim->num_vecs = 0;
    stim->num_unrolled_vecs = 0;
    stim->num_vec_chunks = 0;
    stim->vec_chunks = NULL;

    // no chunks loaded so clear
    stim->cur_vec_chunk_id = -1;

    // number of NOP vecs to pad with so num_vecs 
    // is a multiple of memory bursts
    stim->num_padding_vecs = 0;

    // reset file 
    stim->is_open = false;
    stim->fd = 0;
    stim->fp = NULL;
    stim->file_size = 0;
    stim->map = NULL;
    stim->cur_map_byte = 0;
    stim->is_little_endian = true;

    return stim;
}


/*
 * Also allocates the needed amount of vector
 * chunks based on the num_vecs given and the max chunk size. Note that
 * the chunk is variable in length but always less than or equal to the
 * max chunk size.
 */
struct stim *init_stim(struct stim *stim, struct profile_pin **pins, uint32_t num_pins, uint32_t num_vecs, uint32_t num_unrolled_vecs){
    if(stim == NULL){
        die("error: failed to initialized stim, pointer is NULL\n");
    }

    stim->num_pins = num_pins;
    stim->num_vecs = num_vecs;
    stim->num_unrolled_vecs = num_unrolled_vecs;

    if(stim->num_pins == 0){
        die("error: failed to init stim, num_pins == 0\n");
    }

    if(stim->num_pins > DUT_TOTAL_NUM_PINS){
        die("error: failed to init stim, num_pins > %i\n", DUT_TOTAL_NUM_PINS);
    }

    if(stim->num_vecs == 0){
        die("error: failed to init stim, num_vecs == 0\n");
    }

    // PADDING
    // Number of vectors must be divisible by 8 because there are
    // 8 vectors per burst. A burst must always complete as per the
    // axi spec, so just pad with NOPs.
    if(stim->num_vecs % STIM_NUM_VECS_PER_BURST != 0){
        stim->num_padding_vecs = STIM_NUM_VECS_PER_BURST - (stim->num_vecs % STIM_NUM_VECS_PER_BURST);
        stim->num_vecs += stim->num_padding_vecs;
        stim->num_unrolled_vecs += stim->num_padding_vecs;
    }
    
    // get vector size and total size
    bool last_chunk_partial = false;
    size_t vecs_size = stim->num_vecs*STIM_VEC_SIZE;
    
    // calculate the number of chunks needed based on vecs_size
    if(vecs_size > STIM_CHUNK_SIZE){
        stim->num_vec_chunks = vecs_size / STIM_CHUNK_SIZE;
        if(vecs_size % STIM_CHUNK_SIZE != 0){
            stim->num_vec_chunks++;
            last_chunk_partial = true;
        }
    }else if (vecs_size <= STIM_CHUNK_SIZE){
        stim->num_vec_chunks = 1;
        last_chunk_partial = true;
    }

    if((stim->vec_chunks = (struct vec_chunk**)calloc(stim->num_vec_chunks, sizeof(struct vec_chunk*))) == NULL){
        die("error: failed to calloc stim vec_chunks\n");
    }

    // calculate how many vectors we can fit in a chunk and 
    // create the chunks. 
    uint32_t vecs_per_chunk = 0;
    for(int i=0; i<stim->num_vec_chunks; i++){
        if((i == stim->num_vec_chunks-1) && last_chunk_partial){
            if(vecs_size > STIM_CHUNK_SIZE){
                size_t mod_size = (vecs_size % STIM_CHUNK_SIZE);
                vecs_per_chunk = mod_size/STIM_VEC_SIZE;
            }else if (vecs_size <= STIM_CHUNK_SIZE){
                vecs_per_chunk = vecs_size/STIM_VEC_SIZE;
            }
        } else {
            vecs_per_chunk = STIM_CHUNK_SIZE/STIM_VEC_SIZE;
        }
        stim->vec_chunks[i] = create_vec_chunk(i, vecs_per_chunk);
        stim->vec_chunks[i]->is_loaded = false;
    }

    // make sure only dut_io pins are being used
    // Note: both arrays are allocated to 400 num_pins is checked above
    int j = 0;
    for(int i=0; i<stim->num_pins; i++){
        if(pins[i]->dut_io_id >= 0){
            stim->pins_active[pins[i]->dut_io_id] = true;
            stim->pins[j++] = pins[i];
        }else{
            die("error: failed to init stim, pin '%s' doesn't"
                "have a valid dut_io_id\n", pins[i]->net_name);
        }
    }

    return stim;
}

/*
 * Loads the next available chunk and unloads the previous chunk. Return NULL
 * if there are no more chunks to load or on error. Loading a chunk consists
 * of allocating enough memory for the vectors.
 *
 */
struct vec_chunk *stim_load_next_chunk(struct stim *stim){
    struct vec_chunk *next_chunk = NULL;

    if(stim == NULL){
        die("error: failed to load vec chunk, pointer is NULL\n");
    }
    
    if(stim->type == STIM_TYPE_NONE){
        die("error: failed to load vec chunk, stim type is none\n");
    }

    if(stim->cur_vec_chunk_id != -1){
        stim_unload_chunk(stim->vec_chunks[stim->cur_vec_chunk_id]);
    }

    // last chunk so exit
    if(stim->cur_vec_chunk_id == (stim->num_vec_chunks-1)){
        return NULL;
    }

    stim->cur_vec_chunk_id++;
    next_chunk = stim->vec_chunks[stim->cur_vec_chunk_id];

    // 1 burst is 1024 bytes, 1 vector is 128 bytes, 8 vecs per burst
    uint32_t num_bursts = next_chunk->num_vecs/8;
    next_chunk->vec_data_size = num_bursts*BURST_BYTES;

    if((next_chunk->num_vecs % 8) != 0){
        die("error: num_vecs %d must be divisible by 8 so we don't \
            have partially filled bursts, just pad NOP vecs\n", next_chunk->num_vecs);
    }


    // allocate vecs array
    if((next_chunk->vec_data = (uint8_t *)calloc(next_chunk->vec_data_size, sizeof(uint8_t))) == NULL){
        die("error: failed to calloc vec chunk's vecs\n");
    }

    memset(next_chunk->vec_data, 0xff, next_chunk->vec_data_size);

    // set the size of the vecs in bytes
    // NOTE: this size does not represent the size we dma over since
    // we only store num_pins in each vec, but when we dma we send
    // a full 200 pin vec over to each artix
    next_chunk->size = next_chunk->num_vecs*STIM_VEC_SIZE;

    return next_chunk;
}

/*
 * Unloads a chunk by freeing all memory and clearing the is_loaded flag.
 *
 */
void stim_unload_chunk(struct vec_chunk *chunk){
    if(chunk == NULL){
        die("error: failed to unload chunk, pointer is NULL\n");
    }
    if(chunk->is_loaded == false){
        return;
    }
    free(chunk->vec_data);
    chunk->vec_data = NULL;
    chunk->vec_data_size = 0;
    chunk->size = 0;
    chunk->is_loaded = false;
    return;
}

/*
 * Function who's pointer will be passed to stim_fill_chunk_by_dots and called
 * to get bitstream subvecs when filling the body section of a dots.
 *
 */
static void stim_get_next_bitstream_subvecs(stuct stim *stim, 
        enum subvecs **subvecs, uint32_t *num_subvecs){
    if(stim == NULL || subvecs == NULL || num_subvecs == NULL){
        die("pointer is NULL\n");
    }
    // get next word (D31 to D00)
    uint32_t word = stim_get_next_bitstream_word(stim);

    // converts word to subvecs (D0 to D31 same as profile_pins)
    (*subvecs) = convert_bitstream_word_to_subvecs(word, num_subvecs);

    return;
}

/*
 * fill chunk with data starting at the current vector that needs to be loaded.
 * A chunk is packed after it has been loaded into memory, with data from the
 * source file. This only fills the chunk if open_stim was called and the file
 * type supports mmap loading.
 *
 *
 */
struct vec_chunk *stim_fill_chunk(struct stim *stim, struct vec_chunk *chunk){
    uint32_t start_num_vecs = 0;
    uint32_t end_num_vecs = 0;
    uint32_t num_vecs_to_load = 0;
    struct config *config = NULL;
    bool is_first_chunk = false;
    bool is_last_chunk = false;

    if(stim == NULL){
        die("pointer is NULL\n");
    }
    
    if(chunk == NULL){
        die("pointer is NULL\n");
    }

    printf("filling chunk %i with %i vecs (%zu bytes)...\n", 
            chunk->id, chunk->num_vecs, chunk->size);
    
    if(stim->type == STIM_TYPE_NONE){
        die("error: failed to fill chunk, stim type is none\n");
    }

    if(chunk->id == 0){
        is_first_chunk = true;
    }
    if(chunk->id == (stim->num_vec_chunks-1)){
        is_last_chunk = true;
    }

    if(stim->type == STIM_TYPE_RBT || stim->type == STIM_TYPE_BIN || stim->type == STIM_TYPE_BIT){
        // first chunk so fill it with the config header
        if(is_first_chunk){
            if((config = create_config(CONFIG_TYPE_HEADER, 1, 0)) == NULL){
                die("error: pointer is NULL\n");
            }
            if((chunk = stim_fill_chunk_by_dots(stim, chunk, config->dots, 
                    config->dots->num_dots_vecs, NULL)) == NULL){
                die("error: failed to fill chunk by config vecs\n");
            }
            config = free_config(config);
        } 
    
        // If this is the last chunk, then the number of vectors also includes
        // vecs for the footer so we need to adjust how many words we load.
        // Note, filling chunk for header will increment cur_vec_id.
        start_num_vecs = chunk->cur_vec_id;
        if(is_last_chunk){
            uint32_t footer_num_vecs = get_config_num_vecs_by_type(CONFIG_TYPE_FOOTER);
            end_num_vecs = chunk->num_vecs - footer_num_vecs - stim->num_padding_vecs;
        }else{
            end_num_vecs = chunk->num_vecs;
        }
        num_vecs_to_load = end_num_vecs - start_num_vecs;
         
        // fill chunk with one data word and the corresponding body config
        if((config = create_config(CONFIG_TYPE_BODY, num_vecs_to_load, 0)) == NULL){
            die("error: pointer is NULL\n");
        }
        // TODO: stim->pins must be based on config_tags
        if((chunk = stim_fill_chunk_by_dots(stim, chunk, config->dots, 
                config->dots->num_dots_vecs, &stim_get_next_bitstream_subvecs)) == NULL){
            die("error: failed to pack chunk by config vecs\n");
        }
        config = free_config(config);
    
        // if we're in the last chunk and we loaded all the data from the source
        // file already, then copy the footer after
        if(is_last_chunk){
            printf("number of padding vecs %i\n", stim->num_padding_vecs);
            if((config = create_config(CONFIG_TYPE_FOOTER, 1, stim->num_padding_vecs)) == NULL){
                die("error: pointer is NULL\n");
            }
            if((chunk = stim_fill_chunk_by_dots(stim, chunk, config->dots, 
                    config->dots->num_dots_vecs, NULL)) == NULL){
                die("error: failed to pack chunk by config vecs\n");
            }
            config = free_config(config);
        }
    }else if(stim->type == STIM_TYPE_DOTS){
        die("mmap loading of dots not supported yet\n");
    }else if(stim->type == STIM_TYPE_RAW){
        die("mmap loading of stim not supported yet\n");
    }else {
        die("invalid stim type\n");
    }

    return chunk;
}


/*
 * Given a dots, fills the chunk with given amount of dots vecs to load.
 * The dots has an internal cur_dots_vecs which keeps track of how many
 * vecs were read from the dots. This is called by stim_fill_chunk if
 * the stim file was loaded by mmap. Otherwise, this can be called directly
 * if loading the stim through the api.
 *
 */
struct vec_chunk *stim_fill_chunk_by_dots(struct stim *stim,
        struct vec_chunk *chunk, struct dots *dots, 
        uint32_t num_dots_vecs_to_load,
        void (*get_next_data_subvecs)(struct stim *, enum subvecs **, uint32_t*)){
    struct dots_vec *dots_vec = NULL;
    enum subvecs *data_subvecs = NULL;
    uint32_t num_data_subvecs = 0;
    struct vec *chunk_vec = create_vec();
    
    if(stim == NULL){
        die("error: pointer is NULL\n");
    }

    if(chunk == NULL){
        die("error: pointer is NULL\n");
    }

    if(dots == NULL){
        die("error: pointer is NULL\n");
    }

    if((dots->cur_dots_vec+num_dots_vecs_to_load) > dots->num_dots_vecs){
        die("failed to load chunk; trying to load too many dots vecs 
            %i > %i num_dots_vecs\n", 
            (dots->cur_dots_vec+num_dots_vecs_to_load),
            dots->num_dots_vecs
        );
    }

    while(dots->cur_dots_vec < num_dots_vecs_to_load){ 
        dots_vec = dots->dots_vecs[dots->cur_dots_vec];
        if(dots_vec ==  NULL){
            die("error: failed to get dots_vec by real id %i\n", dots->cur_dots_vec);
        }

        if(dots_vec->num_subvecs != stim->num_pins){
            die("error: num_subvecs %i != num_pins %i\n", 
                dots_vec->num_subvecs, stim->num_pins);
        }

        if(chunk->cur_vec_id > (chunk->num_vecs-1)){
            die("error: cur_vec_id %i exceeded chunk's num_vecs %i\n",
                chunk->cur_vec_id, chunk->num_vecs);
        }

        // clear subvecs
        data_subvecs = NULL;
        num_data_subvecs = 0;

        // get data subvecs if we need to inject them into the vector
        if(get_next_data_subvecs != NULL){
            if(dots_vec->repeat != 1){
                die("error: dots vec for body must have a repeat of one\
                    but it has a repeat of %d\n", dots_vec->repeat);
            }
            (*get_next_data_subvecs)(stim, &data_subvecs, &num_data_subvecs);
            if(data_subvecs == NULL){
                die("error: data subvecs is NULL\n");
            }
        }

        // Convert the dots_vec's vec_str into subvecs. If bitstream inject
        // the data subvecs for the data pins.
        expand_dots_vec_str(dots_vec, data_subvecs, num_data_subvecs); 

        // clear the chunk's vec
        for(int j=0; j<STIM_VEC_SIZE; j++){
            chunk_vec->packed_subvecs[j] = 0xff;
        }

        // pack chunk vec with subvecs (which represent pins)
        for(int pin_id=0; pin_id<dots_vec->num_subvecs; pin_id++){
            struct profile_pin *pin = stim->pins[pin_id];
            enum subvecs subvec = dots_vec->subvecs[pin_id];
            pack_subvecs_by_dut_io_id(chunk_vec->packed_subvecs, pin->dut_io_id, subvec);
        }

        // set the opcode for the chunk vec
        if(dots_vec->has_clk){
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_VECCLK, dots_vec->repeat);
        }else if(dots_vec->repeat > 1){
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_VECLOOP, dots_vec->repeat);
        }else{
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_VEC, dots_vec->repeat);
        }

        // No need to swap the endianess of packed_subvecs because 64 bit words
        // are packed lsb to msb in the 1024 bit word in agent, dutcore and memcore.
        // The zynq fabric dma uses a 64 bit bus and uses little endian, so when it 
        // gets a 64 bit word it will load it in the register big endian and pass that
        // down the wire. So we fill the vector from 0 to 199, but the dma will correctly
        // grab the 64 bit word when it reads memory. Also, the bus is from [1023:0] so 
        // we need to store high dut_io to low dut_io from msb to lsb in the 64 bit word.
        memcpy(chunk->vec_data+(chunk->cur_vec_id*STIM_VEC_SIZE), 
                chunk_vec->packed_subvecs, STIM_VEC_SIZE);
        chunk->cur_vec_id += 1;

        // free data structs
        if(get_next_data_subvecs != NULL && data_subvecs != NULL){
            free(data_subvecs);
            data_subvecs = NULL;
        }
        dots_vec = NULL;

        // increment the dots vec for the next cycle
        dots->cur_dots_vec += 1;
    }

    chunk_vec = free_vec(chunk_vec);

    return chunk;
}


/*
 * Creates a new stim object, mmap the file, and save handles to stim.
 * Path can be a dots, rbt, bin, bit or a raw stim file. Calculates
 * number of vectors and chunks needed based on the file type given.
 *
 *
 */
struct stim *open_stim(const char *profile_path, const char *path){
    struct stim * stim = NULL;
    int fd;
    FILE *fp = NULL;
    off_t file_size = 0;
    off_t bitstream_size = 0;
    struct profile *profile = NULL;
    uint32_t num_vecs = 0;
    uint32_t num_unrolled_vecs = 0;
    uint32_t num_pins = 0;
    struct profile_pin **pins = NULL;
    char buffer[BUFFER_LENGTH];

    if((stim = create_stim()) == NULL){
        die("error: pointer is NULL\n");
    }

    profile = get_profile_by_path(profile_path);
    if(profile == NULL){
        die("error: pointer is NULL\n");
    }

    if(util_fopen(path, &fd, &fp, &file_size)){
        die("error: failed to open file '%s'\n", path);
    }
    
    const char *file_ext = util_get_file_ext_by_path(path);
    if(strcmp(file_ext, "rbt") == 0){
        stim->type = STIM_TYPE_RBT;
    }else if(strcmp(file_ext, "bin") == 0){
        stim->type = STIM_TYPE_BIN;
    }else if(strcmp(file_ext, "bit") == 0){
        stim->type = STIM_TYPE_BIT;
    }else if(strcmp(file_ext, "dots") == 0){
        stim->type = STIM_TYPE_DOTS;
    }else if(strcmp(file_ext, "stim") == 0){
        stim->type = STIM_TYPE_RAW;
    }else{
        die("error: invalid file type given '%s'\n", file_ext);
    }

    // save file handle data to stim so we can load chunks as needed
    stim->is_open = true;
    stim->fd = fd;
    stim->fp = fp;
    stim->file_size = file_size;
    stim->map = (uint8_t*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0); 
    stim->cur_map_byte = 0;

    if(stim->map == MAP_FAILED){
        close(stim->fd);
        die("error: failed to map file\n");
    }

    // Find the endianness of the bitstream
    if(stim->type == STIM_TYPE_RBT){
        stim->is_little_endian = false;
    }else if(stim->type == STIM_TYPE_BIN || stim->type == STIM_TYPE_BIT){
        bool found = false;
        while(1){
            uint32_t word = read_map_32(stim, false);
            if(word == 0xaa995566){
                stim->is_little_endian = true;
                found = true;
                break;
            }else if (word == 0x665599aa){
                stim->is_little_endian = false;
                found = true;
                break;
            }
        }
        if(!found){
            die("invalid bitstream '%s'; failed to find sync word", path);
        }else{
            stim->cur_map_byte = 0;
        }
    }

    // read rbt header and get number of bits from 7th line
    if(stim->type == STIM_TYPE_RBT){
        int32_t c = 0;
        uint32_t line = 0;
        uint32_t count = 0;
        uint32_t word = 0;
        while(cur_map_byte < file_size){
            if(stim->map[stim->cur_map_byte+count] == '\n'){
                if(count > BUFFER_LENGTH){
                    die("buffer overflow; gross\n");
                }
                memset(buffer, '\0', BUFFER_LENGTH);
                memcpy(buffer, (stim->map+stim->cur_map_byte), count);
                stim->cur_map_byte += (count+1);
                if((line++) == 6){
                    while(c < BUFFER_LENGTH){
                        if(isdigit(buffer[c++])){
                            bitstream_size = atoi(&buffer[c-1]);
                            printf("bitstream size: %i\n", bitstream_size);
                            break;
                        }
                    }
                    break;
                }
                count = 0;
            }else{
                count += 1;
            }
        }
        if(bitstream_size == 0){
            die("failed to read rbt size from header; it's zero\n");
        }
    // bin bitstream size is exactly the file size since no header
    }else if(stim->type == STIM_TYPE_BIN){
        bitstream_size = file_size;
    // read bit header and get number of bits
    } else if(stim->type == STIM_TYPE_BIT){
        uint8_t byte = 0;
        uint16_t halfword = 0;
        uint32_t word = 0;

        // check for <0009> header
        halfword = read_map_16(stim, true);
        if(halfword != 9){
            die("missing <0009> header, not a bit file");
        }
        stim->cur_map_byte += halfword;

        // check for <0001> header
        halfword = read_map_16(stim, true);
        if(halfword != 1){
            die("missing <0001> header, not a bit file");
        }

        // check <a> design name header
        byte = read_map_8(stim);
        if(((char)byte) != 'a'){
            die("missage <a> header, not a bit file");
        }

        // get design name
        halfword = read_map_16(stim, true);
        if(halfword > BUFFER_LENGTH){
            die("design name longer than BUFFER_LENGTH bytes; don't like that")
        }

        // copy name into buffer
        memset(buffer, '\0', BUFFER_LENGTH);
        memcpy(buffer, (stim->map+stim->cur_map_byte), halfword);
        stim->cur_map_byte += halfword;
        printf("Design name: %s\n", buffer);

        // read the header and break when body is reached
        while(1){
            byte = read_map_8(stim);
            // e is bitstream data
            if(((char)byte) == 'e'){
                bitstream_size = read_map_32(stim, true);
                printf("bitstream has %i bytes...\n", bitstream_size);
                // Header has been read. Ready to read bitstream.
                break;
            // b is partname, c is date, d is time
            }else if(((char)byte) == 'b' || ((char)byte) == 'c' || ((char)byte) == 'd'){
                halfword = read_map_16(stim, true);
                if(halfword > BUFFER_LENGTH){
                    die("design name longer than BUFFER_LENGTH bytes; don't like that\n");
                }
                memset(buffer, '\0', BUFFER_LENGTH);
                memcpy(buffer, (stim->map+stim->cur_map_byte), halfword);
                stim->cur_map_byte += halfword;
                printf("%s\n", buffer);
            } else {
                die("failed reading bit file; unexpected key\n");
            }
        }
    }

    // get the pins, num_pins and num_vecs for each file type
    switch(stim->type){
        case STIM_TYPE_NONE:
            die("error: no stim type set\n");
            break;
        case STIM_TYPE_RBT:
        case STIM_TYPE_BIN:
        case STIM_TYPE_BIT:
            if(bitstream_size == 0){
                die("failed to get bitstream size; size is zero\n");
            }
            if((pins = get_config_profile_pins(profile, &num_pins)) == NULL){
                die("error: failed to get profile config pins\n");
            }
            // need to prep fpga for rbt and need to do post config checks
            num_vecs += get_config_num_vecs_by_type(CONFIG_TYPE_HEADER);
            num_vecs += get_config_num_vecs_by_type(CONFIG_TYPE_FOOTER);

            // get num taking into account repeat value and clocking (double)
            num_unrolled_vecs += get_config_unrolled_num_vecs_by_type(CONFIG_TYPE_HEADER);
            num_unrolled_vecs += get_config_unrolled_num_vecs_by_type(CONFIG_TYPE_FOOTER);

            // bin files have no header, it's just raw words ready to use.
            uint32_t num_file_vecs = (bitstream_size/sizeof(uint32_t));
            if(bitstream_size % sizeof(uint32_t) != 0){
                die("error: bitstream given '%s' is not 32 bit word aligned\n", path);
            }
            uint32_t num_body_vecs = num_file_vecs*get_config_num_vecs_by_type(CONFIG_TYPE_BODY);
            num_vecs += num_body_vecs; 

            uint32_t num_unrolled_body_vecs = num_file_vecs*get_config_unrolled_num_vecs_by_type(CONFIG_TYPE_BODY);
            num_unrolled_vecs += num_unrolled_body_vecs;
            break;
        case STIM_TYPE_DOTS:
            die("error: dots not supported yet\n");
            break;
        case STIM_TYPE_RAW:
            die("error: stim not supported yet\n");
            break;
        default:
            die("error: failed to handle stim type\n");
            break;
    }

    // initialize the stim with the pins and vec from the parsed file
    if((stim = init_stim(stim, pins, num_pins, num_vecs, num_unrolled_vecs)) == NULL){
        die("error: pointer is NULL\n");
    }

    // auto-close stim on exit
    on_exit(&close_stim, (void*)stim);

    return stim;
}

/*
 * Close and un-map the stim's file. No need to call
 * this directly. It's called by on_exit.
 *
 */
void close_stim(int status, void *vstim){
    struct stim *stim = vstim;
    if(stim == NULL){
        die("error: pointer is NULL\n");
    }
    if(stim->is_open == false){
        fprintf(stderr, "warning: no need to close stim manually, done automatically\n");
        return;
    }
    if(munmap(stim->map, stim->file_size) == -1){
        die("error: failed to munmap file\n");
    }
    fclose(stim->fp);
    close(stim->fd);

    // clear handles
    stim->is_open = false;
    stim->fd = 0;
    stim->fp = NULL;
    stim->file_size = 0;
    stim->map = NULL;
    stim->map_bye_offset = 0;
    stim->is_little_endian = true;
    return;
}

/*
 * Free vec object memory.
 *
 */
struct vec * free_vec(struct vec *vec){
    if(vec == NULL){
        return NULL;
    }
    if(vec->packed_subvecs == NULL){
        die("warning: failed to free vec, packed_subvecs is NULL\n");
    }
    free(vec->packed_subvecs);
    vec->packed_subvecs = NULL;
    free(vec);
    return NULL;
}

/*
 * Free vec chunk object memory.
 *
 */
struct vec_chunk * free_vec_chunk(struct vec_chunk *chunk){
    if(chunk == NULL){
        return NULL;
    }
    free(chunk);
    return NULL;
}

/*
 * Free stim object memory.
 *
 */
struct stim * free_stim(struct stim *stim){
    if(stim == NULL){
        return NULL;
    }
    if(stim->pins == NULL){
        die("warning: failed to free stim, pins is NULL\n");
    }

    if(stim->vec_chunks == NULL){
        die("warning: failed to free stim, vecs is NULL\n");
    }
    stim->pins = free_profile_pins(stim->pins, stim->num_pins);
    for(int i=0; i<stim->num_vec_chunks; i++){
        stim->vec_chunks[i] = free_vec_chunk(stim->vec_chunks[i]);
    }
    free(stim->vec_chunks);
    stim->vec_chunks = NULL;
    free(stim);
    return NULL;
}



