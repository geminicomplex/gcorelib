/*
 * Stim File
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#include "common.h"
#include "subvec.h"
#include "stim.h"
#include "util.h"
#include "profile.h"
#include "config.h"
#include "serialize/stim_serdes.capnp.h"
#include "lib/capnp/capnp_c.h"
#include "lib/lz4/lz4.h"
//#include "../driver/gcore_common.h"

#include <limits.h>
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
#include <ctype.h>

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
 * Convert c string to capn_text;
 *
 */
static capn_text chars_to_text(const char *chars) {
  return (capn_text) {
    .len = (int) strlen(chars),
    .str = chars,
    .seg = NULL,
  };
}

/*
 * capn_write_fd write callback funciton.
 *
 */
#ifdef VERILATOR
ssize_t write_capn_fd(int fd, void *p, size_t count){
#else
ssize_t write_capn_fd(int fd, const void *p, size_t count){
#endif
    ssize_t bytes = 0;
    bytes = write(fd, p, count);
    return bytes;
}

/*
 * Calculates number of padding vecs needed based on number of
 * vectors given. 
 *
 * Number of vectors must be divisible by 8 because there are
 * 8 vectors per burst. A burst must always complete as per the
 * axi spec, so just pad with NOPs
 *
 */
uint32_t calc_num_padding_vecs(uint32_t num_vecs){
    uint32_t num_padding_vecs = 0;
    if(num_vecs % STIM_NUM_VECS_PER_BURST != 0){
        num_padding_vecs = STIM_NUM_VECS_PER_BURST - (num_vecs % STIM_NUM_VECS_PER_BURST);
    }
    return num_padding_vecs;
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
        die("pointer is NULL");
    }

    if((subvecs = (enum subvecs *)calloc(num_bits, sizeof(enum subvecs))) == NULL){
        die("error: failed to calloc subvecs");
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
        die("pointer is NULL");
    }
    uint8_t *byte = (uint8_t*)&(stim->map)[stim->cur_map_byte];
    if(byte == NULL){
        die("map byte is NULL");
    }
    stim->cur_map_byte += 1;
    return *byte;
}

static inline uint16_t read_map_16(struct stim *stim, bool swap_endian){
    if(stim == NULL){
        die("pointer is NULL");
    }
    uint16_t *halfword = (uint16_t*)&(stim->map)[stim->cur_map_byte];
    if(halfword == NULL){
        die("map halfword is NULL");
    }
    stim->cur_map_byte += 2;
    if(swap_endian){
        return bswap_16(*halfword);
    }
    return *halfword;
}

static inline uint32_t read_map_32(struct stim *stim, bool swap_endian){
    if(stim == NULL){
        die("pointer is NULL");
    }
    uint32_t *word = (uint32_t*)&(stim->map)[stim->cur_map_byte];
    if(word == NULL){
        die("map word is NULL");
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
    int32_t c = 0;
    uint32_t count = 0;
    switch(stim->type){
        case STIM_TYPE_RBT:
            while(stim->cur_map_byte < stim->file_size){
                if(stim->map[stim->cur_map_byte+count] == '\n'){
                    if(count > BUFFER_LENGTH){
                        die("buffer overflow; gross");
                    }
                    memset(buffer, '\0', BUFFER_LENGTH);
                    memcpy(buffer, (stim->map+stim->cur_map_byte), count);
                    if(count != 32){
                        die("rbt bitstream word is not 32 bits");
                    }
                    c = count;
                    word = 0;
                    while((--c) >= 0){
                        if(buffer[c] == '0'){
                            continue;
                        }else if(buffer[c] == '1'){
                            word += (1<<((count-1)-c));
                        }else{
                            die("invalid rbt bitstream");
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
                return 0;
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
            die("error: file is not a bitstream");
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
        die("error: stim vec size %d must be a multiple of 2", STIM_VEC_SIZE);
    }
   
    if((vec->packed_subvecs = (uint8_t*)calloc(STIM_VEC_SIZE, sizeof(uint8_t))) == NULL){
        die("error: failed to calloc vec's subvecs uint8_t");
    }

    for(int i=0; i<STIM_VEC_SIZE; i++){
        vec->packed_subvecs[i] = 0xff;
    }
    return vec;
}

/*
 * Creates an array of vec chunk pointers.
 *
 */
struct vec_chunk **create_vec_chunks(uint32_t num_vec_chunks){
    struct vec_chunk **vec_chunks = NULL;
    if(num_vec_chunks == 0){
        return NULL;
    }
    if((vec_chunks = (struct vec_chunk**)calloc(num_vec_chunks, sizeof(struct vec_chunk*))) == NULL){
        die("error: failed to calloc stim vec_chunks");
    }
    return vec_chunks;
}

/* 
 * Create new vec_chunk object. Because we can only store so many vectors 
 * in memory at a time, we split them into chunks and load them as needed
 * from an mmap source. 
 *
 */
struct vec_chunk *create_vec_chunk(uint8_t vec_chunk_id, 
        enum artix_selects artix_select, uint32_t num_vecs){
    struct vec_chunk *chunk = NULL;


    if(artix_select == ARTIX_SELECT_NONE){
        die("failed to create vec chunk; artix select cannot be none");
    }else if(artix_select == ARTIX_SELECT_BOTH){
        die("failed to create vec chunk; artix select cannot be both");
    }

    if(num_vecs == 0){
        die("error: num_vecs == 0, failed to malloc vec_chunk struct");
    }

    if((chunk = (struct vec_chunk*)malloc(sizeof(struct vec_chunk))) == NULL){
        die("error: failed to malloc vec_chunk struct\n.");
    }

    chunk->id = vec_chunk_id;
    chunk->artix_select = artix_select;
    chunk->num_vecs = num_vecs;

    // don't malloc until we load the chunk to save on memory
    chunk->cur_vec_id = 0;

    // raw compiled vectors that get DMAed to artix units
    chunk->vec_data = NULL;

    // 1 burst is 1024 bytes, 1 vector is 128 bytes, 8 vecs per burst
    uint32_t num_bursts = chunk->num_vecs/8;
    chunk->vec_data_size = num_bursts*BURST_BYTES;

    if((chunk->num_vecs % 8) != 0){
        die("error: chunk num_vecs %d must be divisible by 8 so we don't \
            have partially filled bursts, just pad NOP vecs", chunk->num_vecs);
    }

    // If raw stim, pointer to lz4 compressed data on disk
    // from capn. Do not free this pointer.
    chunk->vec_data_compressed = NULL;
    chunk->vec_data_compressed_size = 0;
    
    chunk->is_loaded = false;
    chunk->is_filled = false;

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

    stim->path = NULL;
    stim->pins = NULL;
    stim->num_pins = 0;

    stim->type = STIM_TYPE_NONE;
    stim->num_vecs = 0;
    stim->num_unrolled_vecs = 0;
    stim->num_a1_vec_chunks = 0;
    stim->num_a2_vec_chunks = 0;
    stim->a1_vec_chunks = NULL;
    stim->a2_vec_chunks = NULL;

    // no chunks loaded so clear
    stim->cur_a1_vec_chunk_id = -1;
    stim->cur_a2_vec_chunk_id = -1;

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

    // Set if STIM_TYPE_DOTS. If loading from file, will
    // convert to dots struct, else if get stim by dots,
    // will get set there.  
    // 
    stim->dots = NULL;

    return stim;
}


/*
 * Also allocates the needed amount of vector chunks based on the num_vecs
 * given and the max chunk size. Note that the chunk is variable in length but
 * always less than or equal to the max chunk size. 
 *
 * We pass pins and num_pins because when the stim gets serialized, stim->dots won't get
 * serialized, if it even is set and we want to save the pins to validate when we 
 * de-serialize.
 *
 */
struct stim *init_stim(struct stim *stim, struct profile_pin **pins, uint32_t num_pins, 
        uint32_t num_vecs, uint64_t num_unrolled_vecs){
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    if(stim == NULL){
        die("error: failed to initialized stim, pointer is NULL");
    }

    stim->num_pins = num_pins;
    stim->num_vecs = num_vecs;
    stim->num_unrolled_vecs = num_unrolled_vecs;

    if(pins == NULL){
        die("pointer is NULL;");
    }

    if(stim->num_pins == 0){
        die("error: failed to init stim, num_pins == 0");
    }

    if(stim->num_pins > DUT_TOTAL_NUM_PINS){
        die("error: failed to init stim, num_profile_pins %i > %i", stim->num_pins, DUT_TOTAL_NUM_PINS);
    }

    if(stim->num_vecs > MAX_NUM_VECS){
        die("number of vectors cannot exceed %" PRIu64"", MAX_NUM_VECS);
    }

    if(stim->num_unrolled_vecs > MAX_NUM_UNROLLED_VECS){
        die("number of unrolled vectors cannot exceed %" PRIu64"", MAX_NUM_UNROLLED_VECS);
    }

    if(stim->num_unrolled_vecs > MAX_NUM_UNROLLED_VECS_WARNING){
        slog_warn(0, "number of unrolled vectors is over %" PRIu64", "
            "which will run for over 10 mins. Are you sure?", MAX_NUM_UNROLLED_VECS_WARNING);
    }

    // Check if pins given are for both units or for either a1 or a2.
    // If both, then we double the number of chunks, each half going
    // to either unit.
    artix_select = get_artix_select_by_profile_pins(pins, num_pins);

    if(artix_select == ARTIX_SELECT_NONE){
        die("pins given don't have any dut_io pins");
    }

    if((stim->pins = (struct profile_pin **)calloc(stim->num_pins, sizeof(struct profile_pin*))) == NULL){
        die("error: failed to calloc profile pins");
    }

    // Copy pins to stim. Freeing the pins passed in is not our responsibility.
    for(uint32_t i=0, j=0; i<stim->num_pins; i++){
        if(pins[i]->dut_io_id >= 0){
            stim->pins[j++] = create_profile_pin_from_pin(pins[i]);
        }else{
            die("error: failed to init stim, pin '%s' doesn't"
                "have a valid dut_io_id", pins[i]->net_name);
        }
    }

    if(stim->num_vecs == 0){
        die("error: failed to init stim, num_vecs == 0");
    }

    stim->num_padding_vecs = calc_num_padding_vecs(stim->num_vecs);
    
    // get vector size and total size
    bool last_chunk_partial = false;
    uint64_t vecs_size = ((uint64_t)stim->num_vecs+(uint64_t)stim->num_padding_vecs)*(uint64_t)STIM_VEC_SIZE;

    // calculate the number of chunks needed based on vecs_size
    uint32_t num_vec_chunks = 0;
    if(vecs_size > STIM_CHUNK_SIZE){
        num_vec_chunks = (uint32_t)(vecs_size / STIM_CHUNK_SIZE);
        if(vecs_size % STIM_CHUNK_SIZE != 0){
            num_vec_chunks++;
            last_chunk_partial = true;
        }
    }else if (vecs_size <= STIM_CHUNK_SIZE){
        num_vec_chunks = 1;
        last_chunk_partial = true;
    }

    // if stim is for both a1 and a2 then when need to double the number
    if(artix_select == ARTIX_SELECT_A1){
        stim->num_a1_vec_chunks = num_vec_chunks;
        stim->num_a2_vec_chunks = 0;
    }else if(artix_select == ARTIX_SELECT_A2){
        stim->num_a1_vec_chunks = 0;
        stim->num_a2_vec_chunks = num_vec_chunks;
    }else if(artix_select == ARTIX_SELECT_BOTH){
        stim->num_a1_vec_chunks = num_vec_chunks;
        stim->num_a2_vec_chunks = num_vec_chunks;
    }else{
        die("artix select %i not allowed", artix_select);
    }

    // allocate the array
    if(stim->num_a1_vec_chunks > 0){
        if((stim->a1_vec_chunks = create_vec_chunks(stim->num_a1_vec_chunks)) == NULL){
            die("pointer is NULL");
        }
    }
    if(stim->num_a2_vec_chunks > 0){
        if((stim->a2_vec_chunks = create_vec_chunks(stim->num_a2_vec_chunks)) == NULL){
            die("pointer is NULL");
        }
    }

    // Calculate how many vectors we can fit in a chunk and 
    // create the chunks. If stim is for both units, then
    // we have double the chunks so fill each half.
    uint32_t vecs_per_chunk = 0;
    for(int i=0; i<num_vec_chunks; i++){
        if((i == num_vec_chunks-1) && last_chunk_partial){
            if(vecs_size > STIM_CHUNK_SIZE){
                size_t mod_size = (vecs_size % STIM_CHUNK_SIZE);
                vecs_per_chunk = (uint32_t)(mod_size/STIM_VEC_SIZE);
            }else if (vecs_size <= STIM_CHUNK_SIZE){
                vecs_per_chunk = (uint32_t)(vecs_size/STIM_VEC_SIZE);
            }
        } else {
            vecs_per_chunk = STIM_CHUNK_SIZE/STIM_VEC_SIZE;
        }

        if(artix_select == ARTIX_SELECT_A1){
            stim->a1_vec_chunks[i] = create_vec_chunk(i, ARTIX_SELECT_A1, vecs_per_chunk);
        }else if(artix_select == ARTIX_SELECT_A2){
            stim->a2_vec_chunks[i] = create_vec_chunk(i, ARTIX_SELECT_A2, vecs_per_chunk);
        }else if(artix_select == ARTIX_SELECT_BOTH){
            stim->a1_vec_chunks[i] = create_vec_chunk(i, ARTIX_SELECT_A1, vecs_per_chunk);
            stim->a2_vec_chunks[i] = create_vec_chunk(i, ARTIX_SELECT_A2, vecs_per_chunk);
        }else{
            die("artix select %i not allowed", artix_select);
        }
    }

    slog_info(0,"num_vecs: %i num_unrolled_vecs: %llu", 
            (stim->num_vecs+stim->num_padding_vecs), 
            (stim->num_unrolled_vecs+(uint64_t)(stim->num_padding_vecs)));

    return stim;
}

/*
 * Loads the next available chunk and unloads the previous chunk. Return NULL
 * if there are no more chunks to load or on error. Loading a chunk consists
 * of allocating enough memory for the vectors. Calling this will also fill
 * the chunk.
 *
 * Each chunk corresponds to a specific artix unit. If you start loading chunks
 * for a unit, you must finished loading before you starting loading another.
 *
 */
struct vec_chunk *stim_load_next_chunk(struct stim *stim, enum artix_selects artix_select){
    struct vec_chunk *next_chunk = NULL;
    uint32_t num_vec_chunks = 0;
    struct vec_chunk **vec_chunks = NULL;
    uint32_t cur_vec_chunk_id = -1;

    if(stim == NULL){
        die("error: failed to load vec chunk, pointer is NULL");
    }
    
    if(stim->type == STIM_TYPE_NONE){
        die("error: failed to load vec chunk, stim type is none");
    }

    if(artix_select == ARTIX_SELECT_NONE){
        die("no artix unit selected");
    }else if(artix_select == ARTIX_SELECT_A1){
        num_vec_chunks = stim->num_a1_vec_chunks;
        vec_chunks = stim->a1_vec_chunks;
        cur_vec_chunk_id = stim->cur_a1_vec_chunk_id;

        if(stim->cur_a2_vec_chunk_id >= 0){
            die("failed to load next chunk for a1, a2 is currently being loaded");
        }
    }else if(artix_select == ARTIX_SELECT_A2){
        num_vec_chunks = stim->num_a2_vec_chunks;
        vec_chunks = stim->a2_vec_chunks;
        cur_vec_chunk_id = stim->cur_a2_vec_chunk_id;

        if(stim->cur_a1_vec_chunk_id >= 0){
            die("failed to load next chunk for a2, a1 is currently being loaded");
        }
    }else if(artix_select == ARTIX_SELECT_BOTH){
        die("cannot select both artix units");
    }

    // reset mmap pointer since loading first chunk
    if(cur_vec_chunk_id == -1){
        stim->cur_map_byte = stim->start_map_byte;
    }

    if(cur_vec_chunk_id != -1){
        if(!vec_chunks[cur_vec_chunk_id]->is_loaded){
            slog_warn(0,"warning: current chunk %i has never been loaded;"
                    " failed to unload. Don't unload manually.", cur_vec_chunk_id);
        }else{
            stim_unload_chunk(vec_chunks[cur_vec_chunk_id]);
        }
    }

    // last chunk so reset cur vec chunk id
    if(cur_vec_chunk_id == (num_vec_chunks-1)){
        cur_vec_chunk_id = -1;
    } else {
        cur_vec_chunk_id += 1;
    }

    // save the id
    if(artix_select == ARTIX_SELECT_A1){
        stim->cur_a1_vec_chunk_id = cur_vec_chunk_id;
    }else if(artix_select == ARTIX_SELECT_A2){
        stim->cur_a2_vec_chunk_id = cur_vec_chunk_id;
    }

    // last chunk so exit
    if(cur_vec_chunk_id == -1){
        return NULL;
    }

    // get the next chunk
    next_chunk = vec_chunks[cur_vec_chunk_id];

    // allocate vecs array
    if((next_chunk->vec_data = (uint8_t *)calloc(next_chunk->vec_data_size, sizeof(uint8_t))) == NULL){
        die("error: failed to calloc vec chunk's vecs");
    }

    // subvecs with 0xff don't get processed by artix units
    memset(next_chunk->vec_data, 0xff, next_chunk->vec_data_size);

    next_chunk->is_loaded = true;
    
    if((next_chunk = stim_fill_chunk(stim, next_chunk)) == NULL){
        die("failed to fill chunk");
    }

    return next_chunk;
}

/*
 * Unloads a chunk by freeing all memory and clearing the is_loaded flag.
 *
 */
void stim_unload_chunk(struct vec_chunk *chunk){
    if(chunk == NULL){
        die("error: failed to unload chunk, pointer is NULL");
    }
    if(chunk->is_loaded == false){
        return;
    }
    chunk->cur_vec_id = 0;
    if(chunk->vec_data != NULL){
        free(chunk->vec_data);
        chunk->vec_data = NULL;
    }
    chunk->is_loaded = false;
    chunk->is_filled = false;
    return;
}

/*
 * Function who's pointer will be passed to stim_fill_chunk_by_dots and called
 * to get bitstream subvecs when filling the body section of a dots.
 *
 */
static void stim_get_next_bitstream_subvecs(struct stim *stim, 
        enum subvecs **subvecs, uint32_t *num_subvecs){
    if(stim == NULL || subvecs == NULL || num_subvecs == NULL){
        die("pointer is NULL");
    }
    // get next word (D31 to D00)
    uint32_t word = stim_get_next_bitstream_word(stim);

    // converts word to subvecs (D0 to D31 same as profile_pins)
    (*subvecs) = convert_bitstream_word_to_subvecs(&word, num_subvecs);

    return;
}

/*
 * fill chunk with data starting at the current vector that needs to be
 * loaded. A chunk is packed after it has been loaded into memory, with data
 * from the source file. Don't call stim_fill_chunk_by_dots directly, but call
 * this instead. 
 *
 * We pre-calculate the size of the chunk based on the number of vecs, taking
 * into account header, body and footer if stim is a bitstream. No need to worry
 * about not having space.
 *
 */
struct vec_chunk *stim_fill_chunk(struct stim *stim, struct vec_chunk *chunk){
    uint32_t start_num_vecs = 0;
    uint32_t end_num_vecs = 0;
    uint32_t num_vecs_to_load = 0;
    struct config *config = NULL;
    bool is_first_chunk = false;
    bool is_last_chunk = false;
    char a1_or_a2_str[5] = "none";

    if(stim == NULL){
        die("pointer is NULL");
    }
    
    if(chunk == NULL){
        die("pointer is NULL");
    }

    if(chunk->is_filled == true){
        die("chunk %i already filled; cannot refill before "
            "calling unload", chunk->id);
    }
    
    if(stim->type == STIM_TYPE_NONE){
        die("error: failed to fill chunk, stim type is none");
    }

    // check if first chunk
    if(chunk->id == 0){
        is_first_chunk = true;
    }

    // check if last chunk
    if(chunk->artix_select == ARTIX_SELECT_A1){
        strncpy(a1_or_a2_str, "a1", 5);
        if(chunk->id == (stim->num_a1_vec_chunks-1)){
            is_last_chunk = true;
        }
    }else if(chunk->artix_select == ARTIX_SELECT_A2){
        strncpy(a1_or_a2_str, "a2", 5);
        if(chunk->id == (stim->num_a2_vec_chunks-1)){
            is_last_chunk = true;
        }
    }else{
        die("invalid chunk artix select %i", chunk->artix_select);
    }
    

    if(is_last_chunk){
        slog_info(0,"filling %s chunk %i with %i vecs (%i padding vecs) (%zu bytes)...", 
            a1_or_a2_str, chunk->id, chunk->num_vecs, stim->num_padding_vecs, chunk->vec_data_size);
    }else{
        slog_info(0,"filling %s chunk %i with %i vecs (%zu bytes)...", 
            a1_or_a2_str, chunk->id, chunk->num_vecs, chunk->vec_data_size);
    }

    if(stim->type == STIM_TYPE_RBT || stim->type == STIM_TYPE_BIN || stim->type == STIM_TYPE_BIT){
        // First chunk so fill it with the config header. Size will always be less than chunk size.
        if(is_first_chunk){
            if((config = create_config(stim->profile, CONFIG_TYPE_HEADER, 1)) == NULL){
                die("error: pointer is NULL");
            }
            if((chunk = stim_fill_chunk_by_dots(stim, chunk, 
                    config->dots, NULL)) == NULL){
                die("error: failed to fill chunk by config vecs");
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
        if((config = create_config(stim->profile, CONFIG_TYPE_BODY, num_vecs_to_load)) == NULL){
            die("error: pointer is NULL");
        }
        if((chunk = stim_fill_chunk_by_dots(stim, chunk, config->dots, 
                &stim_get_next_bitstream_subvecs)) == NULL){
            die("error: failed to pack chunk by config vecs");
        }
        config = free_config(config);
    
        // if we're in the last chunk and we loaded all the data from the source
        // file already, then copy the footer after
        if(is_last_chunk){
            if((config = create_config(stim->profile, CONFIG_TYPE_FOOTER, 1)) == NULL){
                die("error: pointer is NULL");
            }

            // last chunk so we need to pad with NOP vecs
            append_dots_vec_by_nop_vecs(config->dots, stim->num_padding_vecs);

            if((chunk = stim_fill_chunk_by_dots(stim, chunk, config->dots, NULL)) == NULL){
                die("error: failed to pack chunk by config vecs");
            }
            config = free_config(config);
        }
    }else if(stim->type == STIM_TYPE_DOTS){
        if(stim->dots != NULL){
            // last chunk so we need to pad with NOP vecs
            if(is_last_chunk){
                append_dots_vec_by_nop_vecs(stim->dots, stim->num_padding_vecs);
            }

            if((chunk = stim_fill_chunk_by_dots(stim, chunk, stim->dots, NULL)) == NULL){
                die("failed to fill chunk by dots");
            }
        }else{
            die("mmap loading of dots not supported yet");
        }
    }else if(stim->type == STIM_TYPE_RAW){
        if((chunk = stim_decompress_vec_chunk(chunk)) == NULL){
            die("failed to decompress vec chunk");
        }
    }else {
        die("invalid stim type");
    }

    return chunk;
}


/*
 * Given a dots, fills the chunk with given amount of dots vecs to load.
 * The dots has an internal cur_dots_vec_ids which keeps track of how many
 * vecs were read from the dots. This is called by stim_fill_chunk, which
 * will handle both if the file is from mmap or if from a dots struct. Don't
 * call this directly.
 *
 */
struct vec_chunk *stim_fill_chunk_by_dots(struct stim *stim,
        struct vec_chunk *chunk, struct dots *dots, 
        void (*get_next_data_subvecs)(struct stim *, enum subvecs **, uint32_t*)
){
    struct dots_vec *dots_vec = NULL;
    enum subvecs *data_subvecs = NULL;
    uint32_t num_data_subvecs = 0;
    uint32_t range_low = 0;
    uint32_t range_high = 0;
    struct vec *chunk_vec = NULL;

    if(stim == NULL){
        die("error: pointer is NULL");
    }

    if(chunk == NULL){
        die("error: pointer is NULL");
    }

    if(dots == NULL){
        die("error: pointer is NULL");
    }

    if(chunk->is_filled == true){
        die("chunk %i already filled; cannot refill before "
            "calling unload", chunk->id);
    }

    if((chunk_vec = create_vec()) == NULL){
        die("pointer is NULL");
    }

    // Each artix select chunk can be filled with the same dots so need
    // to keep track of which unit we're filling for.
    uint32_t cur_dots_vec_id = 0;
    if(chunk->artix_select == ARTIX_SELECT_A1){
        cur_dots_vec_id = dots->cur_a1_dots_vec_id;
    }else if(chunk->artix_select == ARTIX_SELECT_A2){
        cur_dots_vec_id = dots->cur_a2_dots_vec_id;
    }else{
        die("invalid chunk artix select %i", chunk->artix_select);
    }

    // check for correct dut_io_id based on artix unit 
    if(chunk->artix_select == ARTIX_SELECT_A1){
        range_low = 0;
        range_high = DUT_NUM_PINS-1;
    }else if(chunk->artix_select == ARTIX_SELECT_A2){
        range_low = DUT_NUM_PINS;
        range_high = DUT_TOTAL_NUM_PINS-1;
    }

    //
    // Fill the chunk with as many vectors as possible. When the chunk fills
    // up it will break and exit preserving the cur_dots_vec_id and cur_chunk_vec.
    //
    while(1){ 

        // Only load as many dots vecs as the dots has. If none left then break. 
        if(cur_dots_vec_id >= dots->num_dots_vecs){
            break;
        }

        // Chunk has filled up so bounce. Rest of the vectors will go into
        // the next chunk.
        if(chunk->cur_vec_id >= chunk->num_vecs){
            break;
        }

        dots_vec = dots->dots_vecs[cur_dots_vec_id];
        if(dots_vec ==  NULL){
            die("error: failed to get dots_vec by real id %i", cur_dots_vec_id);
        }

        if(dots_vec->num_subvecs != stim->num_pins){
            die("error: num_subvecs %i != num_pins %i", 
                dots_vec->num_subvecs, stim->num_pins);
        }

        if(chunk->cur_vec_id > (chunk->num_vecs-1)){
            die("error: cur_vec_id %i exceeded chunk's num_vecs %i",
                chunk->cur_vec_id, chunk->num_vecs);
        }

        // clear subvecs
        data_subvecs = NULL;
        num_data_subvecs = 0;

        // get data subvecs if we need to inject them into the vector
        if(get_next_data_subvecs != NULL){
            if(dots_vec->repeat != 1){
                die("error: dots vec for body must have a repeat of one "
                    "but it has a repeat of %d", dots_vec->repeat);
            }
            (*get_next_data_subvecs)(stim, &data_subvecs, &num_data_subvecs);
            if(data_subvecs == NULL){
                die("error: data subvecs is NULL");
            }
        }

        // clear the chunk's vec
        for(int j=0; j<STIM_VEC_SIZE; j++){
            chunk_vec->packed_subvecs[j] = 0xff;
        }

        // Convert the dots_vec's vec_str into subvecs. If bitstream inject
        // the data subvecs for the data pins. Must call this before getting
        // a subvec
        expand_dots_vec_subvecs(dots, dots_vec, data_subvecs, num_data_subvecs); 

        if(dots_vec->subvecs == NULL){
            die("failed to expand dots_vec subvecs");
        }

        // if all subvecs are don't-care then it's a NOP
        bool is_nop_vec = true;

        // pack chunk vec with subvecs (which represent pins)
        for(int pin_id=0; pin_id<dots_vec->num_subvecs; pin_id++){
            struct profile_pin *pin = dots->pins[pin_id];

            // skip past pins that are not in this artix_select
            if(pin->dut_io_id < range_low || pin->dut_io_id > range_high){
                continue;
            }

            enum subvecs subvec = dots_vec->subvecs[pin_id];
            pack_subvecs_by_dut_io_id(chunk_vec->packed_subvecs, pin->dut_io_id, subvec);

            if(subvec != DUT_SUBVEC_X){
                is_nop_vec = false;
            }
        }

        //slog_debug(0, "dots vec %i %s has_clk: %i", dots_vec->repeat, dots_vec->vec_str, dots_vec->has_clk);

        unexpand_dots_vec_subvecs(dots_vec);

        // set the opcode for the chunk vec
        if(dots_vec->has_clk){
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_VECCLK, dots_vec->repeat);
        }else if(dots_vec->repeat > 1){
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_VECLOOP, dots_vec->repeat);
        }else if(is_nop_vec){
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_NOP, dots_vec->repeat);
        }else{
            pack_subvecs_with_opcode_and_operand(chunk_vec->packed_subvecs, DUT_OPCODE_VEC, dots_vec->repeat);
        }

        // No need to swap the endianess of packed_subvecs because 64 bit
        // words are packed lsb to msb in the 1024 bit word in agent, gvpu
        // and memcore. The zynq fabric dma uses a 64 bit bus and uses little
        // endian, so when it gets a 64 bit word it will load it in the
        // register big endian and pass that down the wire. So we fill the
        // vector from 0 to 199, but the dma will correctly grab the 64 bit
        // word when it reads memory. Also, the bus is from [1023:0] so we
        // need to store high dut_io to low dut_io from msb to lsb in the 64
        // bit word.
        memcpy(chunk->vec_data+(chunk->cur_vec_id*STIM_VEC_SIZE), 
                chunk_vec->packed_subvecs, STIM_VEC_SIZE);
        chunk->cur_vec_id += 1;

        // free data structs
        if(get_next_data_subvecs != NULL && data_subvecs != NULL){
            free(data_subvecs);
            data_subvecs = NULL;
        }
        dots_vec = NULL;

        // increment the dots vec for the next cycle and save it
        cur_dots_vec_id += 1;
        if(chunk->artix_select == ARTIX_SELECT_A1){
            dots->cur_a1_dots_vec_id = cur_dots_vec_id;
        }else if(chunk->artix_select == ARTIX_SELECT_A2){
            dots->cur_a2_dots_vec_id = cur_dots_vec_id;
        }
    }

    chunk_vec = free_vec(chunk_vec);

    // check if chunk has been loaded with the full amount of vecs it can hold
    if(chunk->cur_vec_id+1 >= chunk->num_vecs){
        chunk->is_filled = true;
    }

    return chunk;
}

enum stim_types get_stim_type_by_path(const char *path){
    enum stim_types type = STIM_TYPE_NONE;
    const char *file_ext = util_get_file_ext_by_path(path);
    if(strcmp(file_ext, "rbt") == 0){
        type = STIM_TYPE_RBT;
    }else if(strcmp(file_ext, "bin") == 0){
        type = STIM_TYPE_BIN;
    }else if(strcmp(file_ext, "bit") == 0){
        type = STIM_TYPE_BIT;
    }else if(strcmp(file_ext, "s") == 0){
        type = STIM_TYPE_DOTS;
    }else if(strcmp(file_ext, "stim") == 0){
        type = STIM_TYPE_RAW;
    }
    return type;
}


/*
 * Creates a new stim object, mmap the file, and save handles to stim.
 * Path can be a dots, rbt, bin, bit or a raw stim file. Calculates
 * number of vectors and chunks needed based on the file type given.
 *
 *
 */
struct stim *get_stim_by_path(const char *profile_path, const char *path){
    struct stim * stim = NULL;
    int fd;
    FILE *fp = NULL;
    off_t file_size = 0;
    off_t bitstream_size = 0;
    uint32_t num_pins = 0;
    struct profile_pin **pins = NULL;
    uint32_t num_vecs = 0;
    uint64_t num_unrolled_vecs = 0;
    char buffer[BUFFER_LENGTH];
    char *real_path = NULL;
    struct profile *profile = NULL; 

    if(profile_path == NULL){
        die("pointer is NULL");
    }

    if(path == NULL){
        die("pointer is NULL");
    }

    enum stim_types stim_type = get_stim_type_by_path(path); 

    if(stim_type == STIM_TYPE_NONE){
        const char *file_ext = util_get_file_ext_by_path(path);
        die("error: invalid file type given '%s'", file_ext);
    }

    if((profile = get_profile_by_path(profile_path)) == NULL){
        die("error: pointer is NULL");
    }

    // only create a stim if not dots, since for dots the parser
    // returns a dots which we use to create a stim
    if(stim_type == STIM_TYPE_RBT || stim_type == STIM_TYPE_BIN 
            || stim_type == STIM_TYPE_BIT || stim_type == STIM_TYPE_RAW){
        if((stim = create_stim()) == NULL){
            die("error: pointer is NULL");
        }

        stim->type = stim_type;
        stim->profile = profile;

        if(util_fopen(path, &fd, &fp, &file_size)){
            die("error: failed to open file '%s'", path);
        }

        if((real_path = realpath(path, NULL)) == NULL){
            die("invalid stim path '%s'", path);
        }

        // legit path so save it
        stim->path = strdup(real_path);

        // Save file handle data to stim so we can load chunks as needed.
        // The cur_map_byte is where we are currently reading from. The
        // start_map_byte is the location after some header is read where
        // we can reset to and restart reading from without initializing.
        stim->is_open = true;
        stim->fd = fd;
        stim->fp = fp;
        stim->file_size = file_size;
        stim->map = (uint8_t*)mmap(NULL, (size_t)file_size, PROT_READ, MAP_SHARED, fd, 0); 
        stim->cur_map_byte = 0;
        stim->start_map_byte = 0;

        if(stim->map == MAP_FAILED){
            close(stim->fd);
            die("error: failed to map file");
        }
    }

    // Find the endianness of the bitstream
    if(stim->type == STIM_TYPE_RBT){
        stim->is_little_endian = false;
    }else if(stim->type == STIM_TYPE_BIN || stim->type == STIM_TYPE_BIT){
        bool found = false;
        while(1){
            // Don't increment cur_map_byte by 4 bytes since the sync word
            // might not be boundry aligned if it's a bit file.
            uint32_t *word = (uint32_t*)&(stim->map)[stim->cur_map_byte++];
            if(word == NULL){
                die("map word is NULL");
            }else if(*word == 0xaa995566){
                stim->is_little_endian = true;
                found = true;
                break;
            }else if (*word == 0x665599aa){
                stim->is_little_endian = false;
                found = true;
                break;
            }
        }
        if(!found){
            die("invalid bitstream '%s'; failed to find sync word", path);
        }else{
            stim->cur_map_byte = 0;
            stim->start_map_byte = 0;
        }
    }

    // read rbt header and get number of bits from 7th line
    if(stim->type == STIM_TYPE_RBT){
        int32_t c = 0;
        uint32_t line = 0;
        uint32_t count = 0;
        while(stim->cur_map_byte < file_size){
            if(stim->map[stim->cur_map_byte+count] == '\n'){
                if(count > BUFFER_LENGTH){
                    die("buffer overflow; gross");
                }
                memset(buffer, '\0', BUFFER_LENGTH);
                memcpy(buffer, (stim->map+stim->cur_map_byte), count);
                stim->cur_map_byte += (count+1);
                if((line++) == 6){
                    while(c < BUFFER_LENGTH){
                        if(isdigit(buffer[c++])){
                            bitstream_size = atoi(&buffer[c-1]);
                            slog_info(0,"bitstream size: %i", (int)bitstream_size);
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
            die("failed to read rbt size from header; it's zero");
        }

        // save location after header
        stim->start_map_byte = stim->cur_map_byte;

    // bin bitstream size is exactly the file size since no header
    }else if(stim->type == STIM_TYPE_BIN){
        bitstream_size = file_size;
    // read bit header and get number of bits
    } else if(stim->type == STIM_TYPE_BIT){
        uint8_t byte = 0;
        uint16_t halfword = 0;

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
        slog_info(0,"Design name: %s", buffer);

        // read the header and break when body is reached
        while(1){
            byte = read_map_8(stim);
            // e is bitstream data
            if(((char)byte) == 'e'){
                bitstream_size = read_map_32(stim, true);
                slog_info(0,"bitstream has %i bytes...", (int)bitstream_size);
                // Header has been read. Ready to read bitstream.
                break;
            // b is partname, c is date, d is time
            }else if(((char)byte) == 'b' || ((char)byte) == 'c' || ((char)byte) == 'd'){
                halfword = read_map_16(stim, true);
                if(halfword > BUFFER_LENGTH){
                    die("design name longer than BUFFER_LENGTH bytes; don't like that");
                }
                memset(buffer, '\0', BUFFER_LENGTH);
                memcpy(buffer, (stim->map+stim->cur_map_byte), halfword);
                stim->cur_map_byte += halfword;
                slog_info(0,"%s", buffer);
            } else {
                die("failed reading bit file; unexpected key");
            }
        }

        // save location after header
        stim->start_map_byte = stim->cur_map_byte;
    }

    // initialize
    int32_t dut_id = -1;
    uint32_t num_file_vecs = 0;
    uint32_t num_body_vecs = 0;
    uint64_t num_unrolled_body_vecs = 0;

    // get the pins, num_pins and num_vecs for each file type
    switch(stim->type){
        case STIM_TYPE_NONE:
            die("error: no stim type set");
            break;
        case STIM_TYPE_RBT:
        case STIM_TYPE_BIN:
        case STIM_TYPE_BIT:
            if(bitstream_size == 0){
                die("failed to get bitstream size; size is zero");
            }

            // TODO: dut_id = -1 filters by all duts. Which only works if there is one dut.
            //       Pass the correct dut_id when supported multiple-duts.
            dut_id = -1;
            if((pins = get_config_profile_pins(stim->profile, dut_id, &num_pins)) == NULL){
                die("error: failed to get profile config pins");
            }

            // Check if the config vecs exceeds the size of a chunk since the code is not
            // designed to handle that. We control the size of these in config.c but add
            // a check just in case.
            if((get_config_num_vecs_by_type(CONFIG_TYPE_HEADER)*STIM_VEC_SIZE) > STIM_CHUNK_SIZE){
                die("config header size cannot exceed a chunk size %i", STIM_CHUNK_SIZE);
            }

            if((get_config_num_vecs_by_type(CONFIG_TYPE_BODY)*STIM_VEC_SIZE) > STIM_CHUNK_SIZE){
                die("config body size cannot exceed a chunk size %i", STIM_CHUNK_SIZE);
            }

            if((get_config_num_vecs_by_type(CONFIG_TYPE_FOOTER)*STIM_VEC_SIZE) > STIM_CHUNK_SIZE){
                die("config footer size cannot exceed a chunk size %i", STIM_CHUNK_SIZE);
            }
            
            // need to prep fpga for rbt and need to do post config checks
            num_vecs += get_config_num_vecs_by_type(CONFIG_TYPE_HEADER);
            num_vecs += get_config_num_vecs_by_type(CONFIG_TYPE_FOOTER);

            // get num taking into account repeat value and clocking (double)
            num_unrolled_vecs += get_config_unrolled_num_vecs_by_type(CONFIG_TYPE_HEADER);
            num_unrolled_vecs += get_config_unrolled_num_vecs_by_type(CONFIG_TYPE_FOOTER);

            // bin files have no header, it's just raw words ready to use.
            num_file_vecs = (uint32_t)(bitstream_size/sizeof(uint32_t));
            if(bitstream_size % sizeof(uint32_t) != 0){
                die("error: bitstream given '%s' is not 32 bit word aligned", path);
            }
            num_body_vecs = num_file_vecs*get_config_num_vecs_by_type(CONFIG_TYPE_BODY);
            num_vecs += num_body_vecs; 

            num_unrolled_body_vecs = ((uint64_t)num_file_vecs)*get_config_unrolled_num_vecs_by_type(CONFIG_TYPE_BODY);
            num_unrolled_vecs += num_unrolled_body_vecs;

            // must get the profile pins from the particular file we're loading
            if(pins == NULL || num_pins == 0){
                die("failed to get_stim_by_path; no pins were set");
            }

            // initialize the stim with the pins and vec from the parsed file
            if((stim = init_stim(stim, pins, num_pins, num_vecs, num_unrolled_vecs)) == NULL){
                die("error: pointer is NULL");
            }
            break;
        case STIM_TYPE_DOTS:
            {
                struct dots *dots = parse_dots(profile, real_path);
                stim = get_stim_by_dots(profile, dots);
            }
            break;
        case STIM_TYPE_RAW:
            if((stim = stim_deserialize(stim)) == NULL){
                die("failed to deserialize stim");
            }
            break;
        default:
            die("error: failed to handle stim type");
            break;
    }

    return stim;
}

/*
 * Returns a stim from a dots object.
 *
 */
struct stim *get_stim_by_dots(struct profile *profile, struct dots *dots){
    struct stim *stim = NULL;
    uint64_t num_unrolled_vecs = 0;

    if(profile == NULL){
        die("pointer is NULL");
    }

    if(dots == NULL){
        die("pointer is NULL");
    }

    if(dots->pins == NULL || dots->num_pins == 0){
        die("dots has no pins");
    }

    for(uint32_t i=0; i<dots->num_pins; i++){
        if(dots->pins[i] == NULL){
            die("pointer is NULL");
        }
    }

    if(dots->dots_vecs == NULL || dots->num_dots_vecs == 0){
        die("dots has no vevs");
    }

    if(dots->num_dots_vecs > 0){
        for(uint32_t i=0; i<dots->num_dots_vecs; i++){
            if(dots->dots_vecs[i] == NULL){
                die("dots has %i num_dots_vecs, but no vecs were "
                        "appended", dots->num_dots_vecs);
            }
        }
    }

    // reset read dots vec id
    dots->cur_a1_dots_vec_id = 0;
    dots->cur_a2_dots_vec_id = 0;

    if((stim = create_stim()) == NULL){
        die("pointer is NULL");
    }

    stim->profile = profile;
    stim->type = STIM_TYPE_DOTS;
    stim->dots = dots;

    num_unrolled_vecs = get_num_unrolled_dots_vecs(stim->dots);

    if((stim = init_stim(stim, dots->pins, dots->num_pins, 
                dots->num_dots_vecs, num_unrolled_vecs)) == NULL){
        die("error: pointer is NULL");
    }
    
    return stim;
}


/*
 * Free vec object memory.
 *
 */
struct vec *free_vec(struct vec *vec){
    if(vec == NULL){
        return NULL;
    }
    if(vec->packed_subvecs == NULL){
        die("warning: failed to free vec, packed_subvecs is NULL");
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
struct vec_chunk *free_vec_chunk(struct vec_chunk *chunk){
    if(chunk == NULL){
        return NULL;
    }
    stim_unload_chunk(chunk);
    chunk->id = 0;
    chunk->artix_select = ARTIX_SELECT_NONE;
    chunk->num_vecs = 0;
    // don't free vec_data_compressed since it points to
    // the mmap pointer on disk.
    free(chunk);
    return NULL;
}

/*
 * Free stim object memory.
 *
 */
struct stim *free_stim(struct stim *stim){
    if(stim == NULL){
        die("pointer is NULL");
    }

    if(stim->pins == NULL){
        die("pointer is NULL");
    }

    if(stim->num_a1_vec_chunks > 0 && stim->a1_vec_chunks == NULL){
        die("num_a1_vec_chunks is %i but a1_vec_chunks is NULL", stim->num_a1_vec_chunks);
    }

    if(stim->num_a2_vec_chunks > 0 && stim->a2_vec_chunks == NULL){
        die("num_a2_vec_chunks is %i but a2_vec_chunks is NULL", stim->num_a2_vec_chunks);
    }

    if(stim->num_pins > 0 && stim->pins == NULL){
        die("num_pins is %i but pins is NULL", stim->num_pins);
    }

    if(stim->path != NULL){
        free(stim->path);
    }

    // free pins
    for(uint32_t i=0; i<stim->num_pins; i++){
        stim->pins[i] = free_profile_pin(stim->pins[i]);
    }
    free(stim->pins);
    stim->pins = NULL;

    // free a1 chunks
    for(uint32_t i=0; i<stim->num_a1_vec_chunks; i++){
        stim->a1_vec_chunks[i] = free_vec_chunk(stim->a1_vec_chunks[i]);
    }
    free(stim->a1_vec_chunks);
    stim->a1_vec_chunks = NULL;

    // free a2 chunks
    for(uint32_t i=0; i<stim->num_a2_vec_chunks; i++){
        stim->a2_vec_chunks[i] = free_vec_chunk(stim->a2_vec_chunks[i]);
    }
    free(stim->a2_vec_chunks);
    stim->a2_vec_chunks = NULL;

    // close mmap
    if(stim->is_open == true){
        if(munmap(stim->map, (size_t)(stim->file_size)) == -1){
            die("error: failed to munmap file");
        }
        fclose(stim->fp);
        close(stim->fd);
    }

    stim->cur_a1_vec_chunk_id = 0;
    stim->cur_a2_vec_chunk_id = 0;
    stim->num_unrolled_vecs = 0;
    stim->is_open = false;
    stim->fd = 0;
    stim->fp = NULL;
    stim->file_size = 0;
    stim->map = NULL;
    stim->cur_map_byte = 0;
    stim->start_map_byte = 0;
    stim->is_little_endian = true;

    // Don't free dots here. It was passed externally by get_stim_by_dots so
    // it's not our responsibility.
    stim->dots = NULL;

    free_profile(stim->profile);
    stim->profile = NULL;

    free(stim);
    return NULL;
}

static size_t stim_serialize_chunk(struct stim *stim, enum artix_selects artix_select, 
        struct capn_segment *cs, struct SerialStim *serialStim){
    struct vec_chunk *chunk = NULL;
    uint8_t *compressed_data = NULL;
    uint32_t compressed_data_size = 0;
    size_t total_saved_size = 0;

    if(stim == NULL){
        die("pointer is NULL");
    }
    if(cs == NULL){
        die("pointer is NULL");
    }
    if(serialStim == NULL){
        die("pointer is NULL");
    }

    if(artix_select == ARTIX_SELECT_NONE){
        die("no artix unit selected");
    }else if(artix_select == ARTIX_SELECT_BOTH){
        die("cannot select both artix units");
    }

    while((chunk = stim_load_next_chunk(stim, artix_select)) != NULL){

        if(chunk->artix_select == ARTIX_SELECT_BOTH){
            die("cannot serialize chunk with artix select as both");
        }

        struct VecChunk vecChunk = {
            .id = chunk->id,
            .artixSelect = (enum VecChunk_ArtixSelects)chunk->artix_select,
            .numVecs = chunk->num_vecs,
            .vecDataSize = (uint32_t)chunk->vec_data_size,
        };

        int max_dst_size = LZ4_compressBound(chunk->vec_data_size);
        if((compressed_data = (uint8_t*)malloc(max_dst_size)) == NULL){
            die("failed to malloc");
        }
        compressed_data_size = LZ4_compress_default((char*)chunk->vec_data, 
                (char*)compressed_data, chunk->vec_data_size, max_dst_size);

        if(compressed_data_size <= 0){
            die("compression failed");
        }
        if((compressed_data = (uint8_t*)realloc(compressed_data, 
                compressed_data_size)) == NULL){
            die("re-alloc failed");
        }

        if(artix_select == ARTIX_SELECT_A1){
            slog_info(0,"compressed a1 chunk %i by %zu bytes", chunk->id, 
                (chunk->vec_data_size-compressed_data_size));
        }else if(artix_select == ARTIX_SELECT_A2){
            slog_info(0,"compressed a2 chunk %i by %zu bytes", chunk->id, 
                (chunk->vec_data_size-compressed_data_size));
        }
        
        total_saved_size += (chunk->vec_data_size-compressed_data_size);

        // copy the data and set it
        capn_list8 list = capn_new_list8(cs, compressed_data_size);
        capn_setv8(list, 0, (uint8_t*)compressed_data, compressed_data_size);
        capn_data vecData = {
            .p = list.p,
        };
        vecChunk.vecData = vecData;
        if(artix_select == ARTIX_SELECT_A1){
            set_VecChunk(&vecChunk, serialStim->a1VecChunks, chunk->id);
        }else if(artix_select == ARTIX_SELECT_A2){
            set_VecChunk(&vecChunk, serialStim->a2VecChunks, chunk->id);
        }else {
            die("invalid artix select given %i", artix_select);
        }
    }

    return total_saved_size;

}

/*
 * Takes a stim, converts it into capn objects and writes it to
 * a file.
 *
 */
void stim_serialize_to_path(struct stim *stim, const char *path){
    if(stim == NULL){
        die("pointer is NULL");
    }

    if(path == NULL){
        die("pointer is NULL");
    }

    int fd = 0;
    struct capn c;
    capn_init_malloc(&c);
    capn_ptr cr = capn_root(&c);
    struct capn_segment *cs = cr.seg;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) {
        die("failed to open stim path %s for writing", path);
	}

    struct SerialStim serialStim = {
        .type = SerialStim_StimTypes_stimTypeRaw,
        .numPins = stim->num_pins,
        .numVecs = stim->num_vecs,
        .numUnrolledVecs = stim->num_unrolled_vecs,
        .numPaddingVecs = stim->num_padding_vecs,
        .numA1VecChunks = stim->num_a1_vec_chunks,
        .numA2VecChunks = stim->num_a2_vec_chunks,
    };

    //
    // Set the profile pins
    //
    serialStim.pins = new_ProfilePin_list(cs, stim->num_pins);

    for(int i=0; i<stim->num_pins; i++){
        struct profile_pin *profile_pin = stim->pins[i];
        enum ProfilePin_ProfileTags profileTag = ProfilePin_ProfileTags_profileTagNone; 
        switch(profile_pin->tag){
            case PROFILE_TAG_NONE:
                profileTag = ProfilePin_ProfileTags_profileTagNone;
                break;
            case PROFILE_TAG_CCLK:
                profileTag = ProfilePin_ProfileTags_profileTagCclk;
                break;
            case PROFILE_TAG_RESET_B:
                profileTag = ProfilePin_ProfileTags_profileTagResetB;
                break;
            case PROFILE_TAG_CSI_B:
                profileTag = ProfilePin_ProfileTags_profileTagCsiB;
                break;
            case PROFILE_TAG_RDWR_B:
                profileTag = ProfilePin_ProfileTags_profileTagRdwrB;
                break;
            case PROFILE_TAG_PROGRAM_B:
                profileTag = ProfilePin_ProfileTags_profileTagProgramB;
                break;
            case PROFILE_TAG_INIT_B:
                profileTag = ProfilePin_ProfileTags_profileTagInitB;
                break;
            case PROFILE_TAG_DONE:
                profileTag = ProfilePin_ProfileTags_profileTagDone;
                break;
            case PROFILE_TAG_DATA:
                profileTag = ProfilePin_ProfileTags_profileTagData;
                break;
            case PROFILE_TAG_GPIO:
                profileTag = ProfilePin_ProfileTags_profileTagGpio;
                break;
            default:
                die("invalid profile pin tag %i", profile_pin->tag);
        }
    
        struct ProfilePin profilePin = {
            .pinName = chars_to_text(profile_pin->pin_name),
            .compName = chars_to_text(profile_pin->comp_name),
            .netName = chars_to_text(profile_pin->net_name),
            .netAlias = chars_to_text(profile_pin->net_alias),
            .tag = profileTag,
            .tagData = profile_pin->tag_data,
            .dutIoId = profile_pin->dut_io_id,
            .numDests = profile_pin->num_dests,
        };

        profilePin.destDutIds = capn_new_list32(cs, profile_pin->num_dests); 
        profilePin.destPinNames = new_String_list(cs, profile_pin->num_dests);
        
        for(int j=0; j<profile_pin->num_dests; j++){
            capn_set32(profilePin.destDutIds, j, profile_pin->dest_dut_ids[j]);
            struct String string = {
                .string = chars_to_text(profile_pin->dest_pin_names[j]),
            };
            set_String(&string, profilePin.destPinNames, j);
        }

        set_ProfilePin(&profilePin, serialStim.pins, i);

    }

    //
    // Set the vec_chunks
    //
    serialStim.a1VecChunks = new_VecChunk_list(cs, stim->num_a1_vec_chunks);
    serialStim.a2VecChunks = new_VecChunk_list(cs, stim->num_a2_vec_chunks);

    size_t total_saved_size = 0;
    if(stim->num_a1_vec_chunks > 0){
        total_saved_size += stim_serialize_chunk(stim, ARTIX_SELECT_A1, cs, &serialStim);
    }
    if(stim->num_a2_vec_chunks > 0){
        total_saved_size += stim_serialize_chunk(stim, ARTIX_SELECT_A2, cs, &serialStim);
    }

    uint64_t total_uncompressed_bytes = 0;
    for(uint32_t i=0; i<stim->num_a1_vec_chunks; i++){
        total_uncompressed_bytes += (uint64_t)(stim->a1_vec_chunks[i]->vec_data_size);
    }
    for(uint32_t i=0; i<stim->num_a2_vec_chunks; i++){
        total_uncompressed_bytes += (uint64_t)(stim->a2_vec_chunks[i]->vec_data_size);
    }

    slog_info(0,"total uncompressed size: %" PRIu64" bytes", total_uncompressed_bytes);
    slog_info(0,"total compressed size: %" PRIu64" bytes", total_uncompressed_bytes-total_saved_size);
    slog_info(0,"compression saved %" PRIu64" bytes", total_saved_size);

    SerialStim_ptr serialStimPtr = new_SerialStim(cs);
    write_SerialStim(&serialStim, serialStimPtr);
    int setp_ret = capn_setp(capn_root(&c), 0, serialStimPtr.p);
    if(setp_ret != 0){
        die("capn setp failed");
    }
    capn_write_fd(&c, &write_capn_fd, fd, 0 /* packed */);
    capn_free(&c);

    close(fd);
    return;
}

static void deserialize_chunk(struct stim *stim, enum artix_selects artix_select, 
        struct SerialStim *serialStim){
    uint32_t num_vec_chunks = 0;

    if(stim == NULL){
        die("pointer is NULL");
    }

    if(artix_select == ARTIX_SELECT_NONE){
        die("no artix unit selected");
    }else if(artix_select == ARTIX_SELECT_A1){
        num_vec_chunks = stim->num_a1_vec_chunks;

        if(stim->num_a1_vec_chunks > 0 && stim->a1_vec_chunks == NULL){
            die("failed to deserialize chunk; chunks have not been allocated yet");
        }
    }else if(artix_select == ARTIX_SELECT_A2){
        num_vec_chunks = stim->num_a2_vec_chunks;

        if(stim->num_a2_vec_chunks > 0 && stim->a2_vec_chunks == NULL){
            die("failed to deserialize chunk; chunks have not been allocated yet");
        }
    }else if(artix_select == ARTIX_SELECT_BOTH){
        die("cannot select both artix units");
    }

    for(uint32_t i=0; i<num_vec_chunks; i++){
        struct vec_chunk *chunk = NULL;
        struct VecChunk vecChunk;
        if(artix_select == ARTIX_SELECT_A1){
            get_VecChunk(&vecChunk, serialStim->a1VecChunks, i);
        }else if(artix_select == ARTIX_SELECT_A2){
            get_VecChunk(&vecChunk, serialStim->a2VecChunks, i);
        }

        chunk = create_vec_chunk(
            i, (enum artix_selects)vecChunk.artixSelect, vecChunk.numVecs);
        chunk->id = vecChunk.id;
        chunk->artix_select = (enum artix_selects)vecChunk.artixSelect;
        chunk->num_vecs = vecChunk.numVecs;
        chunk->vec_data = NULL;
        chunk->vec_data_size = vecChunk.vecDataSize;
        chunk->vec_data_compressed = (uint8_t*)vecChunk.vecData.p.data;
        chunk->vec_data_compressed_size = vecChunk.vecData.p.len;

        if(artix_select == ARTIX_SELECT_A1){
            stim->a1_vec_chunks[i] = chunk;
        }else if(artix_select == ARTIX_SELECT_A2){
            stim->a2_vec_chunks[i] = chunk;
        }
    }

    return;
}

/*
 * Deserializes a type raw stim file.
 *
 */
struct stim *stim_deserialize(struct stim *stim){
    if(stim == NULL){
        die("pointer is NULL");
    }
    struct capn capn;

    if(stim->is_open == false || stim->map == NULL){
        die("failed to deserialize stim; map is not open");
    }

    if(capn_init_mem(&capn, stim->map, stim->file_size, 0 /* packed */) != 0){
        die("cap init mem failed");
    }

    SerialStim_ptr serialStim_ptr; 
    struct SerialStim serialStim;

    serialStim_ptr.p = capn_getp(capn_root(&capn), 
            0 /* off */, 1 /* resolve */);
    read_SerialStim(&serialStim, serialStim_ptr);

    stim->type = (enum stim_types)serialStim.type;
    stim->num_pins = serialStim.numPins;
    stim->num_vecs = serialStim.numVecs;
    stim->num_unrolled_vecs = serialStim.numUnrolledVecs;
    stim->num_padding_vecs = serialStim.numPaddingVecs;
    stim->num_a1_vec_chunks = serialStim.numA1VecChunks;
    stim->num_a2_vec_chunks = serialStim.numA2VecChunks;

    //
    // Set the profile pins
    //
    stim->pins = create_profile_pins(stim->num_pins);

    for(int i=0; i<stim->num_pins; i++){
        struct ProfilePin profilePin;
        get_ProfilePin(&profilePin, serialStim.pins, i);

        struct profile_pin *pin = create_profile_pin(profilePin.numDests);
        
        pin->pin_name = strndup(profilePin.pinName.str, 
                profilePin.pinName.len);
        pin->comp_name = strndup(profilePin.compName.str,
                profilePin.compName.len);
        pin->net_name = strndup(profilePin.netName.str,
                profilePin.netName.len);
        pin->net_alias = strndup(profilePin.netAlias.str,
                profilePin.netAlias.len);
        pin->tag = (enum profile_tags)profilePin.tag;
        pin->tag_data = profilePin.tagData;
        pin->dut_io_id = profilePin.dutIoId;
        pin->num_dests = profilePin.numDests;

        for(int j=0; j<pin->num_dests; j++){
            pin->dest_dut_ids[j] = capn_get32(profilePin.destDutIds, j);
            struct String string;
            get_String(&string, profilePin.destPinNames, j);
            pin->dest_pin_names[j] = strndup(string.string.str, 
                    string.string.len);
        }

        stim->pins[i] = pin;
    }

    //
    // Set the vec_chunks
    //
    if(stim->num_a1_vec_chunks > 0){
        if((stim->a1_vec_chunks = create_vec_chunks(stim->num_a1_vec_chunks)) == NULL){
            die("pointer is NULL");
        }
        deserialize_chunk(stim, ARTIX_SELECT_A1, &serialStim);
    }
    
    if(stim->num_a2_vec_chunks > 0){
        if((stim->a2_vec_chunks = create_vec_chunks(stim->num_a2_vec_chunks)) == NULL){
            die("pointer is NULL");
        }
        deserialize_chunk(stim, ARTIX_SELECT_A2, &serialStim);
    }

    return stim;
}

/*
 * If the stim file is raw, then the stim struct pointers will be populated
 * with pointers from the MMAPed file when de-serializing the capn structs,
 * since it doesn't copy data.  
 *
 */
struct vec_chunk *stim_decompress_vec_chunk(struct vec_chunk *chunk){
    if(chunk == NULL){
        die("pointer is NULL");
    }
    if(chunk->vec_data == NULL){
        die("failed to decompress chunk; vec_data not allocated");
    }
    if(chunk->vec_data_size == 0){
        die("failed to decompress chunk; vec_data_size is 0");
    }
    if(chunk->vec_data_compressed == NULL){
        die("failed to decompress chunk; chunk is not compressed");
    }
    if(chunk->vec_data_compressed_size == 0){
        die("failed to decompress chunk; compress size is 0");
    }

    int decompressed_size = LZ4_decompress_safe((char*)chunk->vec_data_compressed,
            (char*)chunk->vec_data, chunk->vec_data_compressed_size, chunk->vec_data_size);

    slog_info(0,"decompressed chunk %i (%zu bytes -> %zu bytes)", chunk->id, 
            chunk->vec_data_compressed_size, chunk->vec_data_size);
    if(decompressed_size <= 0){
        die("chunk decompression failed");
    }

    return chunk;
}

/*
 * Returns byte array that we can pass to GVPU TEST_SETUP
 * to enable/disable the DUT IO pins.
 *
 * Make sure you free array after use.
 *
 */
uint8_t *stim_get_enable_pins_data(struct stim *stim, enum artix_selects artix_select){
    uint32_t range_low = 0;
    uint32_t range_high = 0;
    struct profile_pin *pin = NULL;

    if(stim == NULL){
        die("pointer is NULL");
    }

    if(artix_select == ARTIX_SELECT_NONE){
        die("no artix unit selected");
    }

    // check for correct dut_io_id based on artix unit 
    if(artix_select == ARTIX_SELECT_A1){
        range_low = 0;
        range_high = DUT_NUM_PINS-1;
    }else if(artix_select == ARTIX_SELECT_A2){
        range_low = DUT_NUM_PINS;
        range_high = DUT_TOTAL_NUM_PINS-1;
    }

    size_t burst_size = BURST_BYTES;

    // clear all pins by FFing them. This will cause test_run to not 
    // process those pins.
    uint8_t *enable_pins = NULL;
    uint32_t num_enable_pins = 256;
    if((enable_pins = (uint8_t*)calloc(burst_size, sizeof(uint8_t))) == NULL){
        die("error: calloc failed");
    }
    for(int i=0; i<num_enable_pins; i++){
        enable_pins[i] = 0xff;
    }

    // turn on only the pins we're using based on the found dut_io_ids
    for(int pin_id=0; pin_id<stim->num_pins; pin_id++){
        pin = stim->pins[pin_id];

        // skip past pins that are not in this artix_select
        if(pin->dut_io_id < range_low || pin->dut_io_id > range_high){
            continue;
        }

        // clamp the id from 0 to 200 since we're only writing to one
        // dut at a time and so packed_subvecs will always be len of 200
        uint8_t dut_io_id = (uint8_t)(pin->dut_io_id % DUT_NUM_PINS);

        enable_pins[dut_io_id] = 0x00;

#ifdef GEM_DEBUG
        slog_debug(0, "%s : %i", pin->net_alias, dut_io_id);
#endif
    }

    // Note: no need to swap the endianess of enable_pins because of the
    // way 64 bit words are packed in agent, gvpu and memcore
    return enable_pins;
}


