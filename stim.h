/*
 * Stim File
 *
 * A stim file is comprised of multiple chunks. Each chunk holds a number of
 * vecs. Chunks can be in a loaded or unloaded state, meaning that memory is
 * currently allocated to store given pre-set number of vecs. Chunks don't
 * hold vec structs directly, but the packed vec_data itself, which is ready
 * to be DMAed to the artix units. If the vec_data is ready, the chunk is said
 * to be filled.
 *
 * The gemini board has 400 dut io pins that the stim has access to. Each
 * artix unit manages 200 pins. A vector comprises of subvecs, which 
 * corresponds to one of the pins. Each subvec is a nibble or 4 bits. Each
 * vector is 128 bytes or 256 nibbles. The 200 lsb nibbles store the vectors
 * and the remaining 56 nibbles are used for the opcode and operand, which
 * will later be utilized to support more features.
 *
 * In other words, each chunk and its vec_data is only compatible with one
 * artix unit.
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef STIM_H
#define STIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#include "profile.h"
#include "config.h"
#include "dots.h"
#include "board/driver.h"


/*
 * Types
 *
 */

/*
 * stim_types identifies the source type used to create the stim
 *
 * RBT, BIN, and BIT are bitstreams
 * DOTS is an unrolled dots file
 * RAW is a stim file written to disk
 *
 */
enum stim_types {
    STIM_TYPE_NONE,
    STIM_TYPE_RBT,
    STIM_TYPE_BIN,
    STIM_TYPE_BIT,
    STIM_TYPE_DOTS,
    STIM_TYPE_RAW
};


/*
 * Each pin is represented by a subvec, and to conserve space
 * 4 bit subvecs are packed into a byte, thus needing half the
 * space. If odd number of pins, will add one to have enough.
 *
 */
struct vec {
    uint8_t *packed_subvecs;
} __attribute__ ((__packed__));


/*
 * vec_chunk holds a group of vecs, not exceeding STIM_CHUNK_SIZE
 * in total size. Chunks also get dynamically loaded and unloaded
 * as needed, with only one being active at any time. Source files
 * are parsed specific to it's type, and loaded into chunks using
 * mmap.
 *
 * id : id of the chunk
 * artix_select: which artix unit the vec_data is for
 * num_vecs : number of vectors
 * cur_vec_id : when filling vecs, used to inc
 * vec_data : compiled raw vector data that DMAs over
 * vec_data_size : chunk size in bytes, including vecs
 * vec_data_compressed : lz4 compressed byte stream
 * vec_data_compressed_size : if chunk is lz4 compressed this will 
 *                            be the number of bytes.
 * is_loaded : loaded in memory yet
 * is_filled : vecs filled in
 *
 */
struct vec_chunk {
    uint8_t id;
    enum artix_selects artix_select;
    uint32_t num_vecs;
    uint32_t cur_vec_id;
    uint8_t *vec_data;
    size_t vec_data_size;
    uint8_t *vec_data_compressed;
    size_t vec_data_compressed_size;
    bool is_loaded;
    bool is_filled;
};


/*
 * A stimulus object represents a dots, rbt, bin, bit or raw stim file.
 * It's comprised of vec_chunk objects, which get dynamically loaded and
 * unloaded as needed, so as to not fill up memory on the memory constrained
 * Gemini board.
 *
 */
struct stim {
    // TODO: Save the paths for reference when we serialize.
    // TODO: Save the stim api revision.

    // public
    char *path;
    enum stim_types type;
    uint16_t num_pins;
    struct profile_pin **pins;
    uint32_t num_vecs;
    uint64_t num_unrolled_vecs;
    uint32_t num_a1_vec_chunks;
    uint32_t num_a2_vec_chunks;
    struct vec_chunk **a1_vec_chunks;
    struct vec_chunk **a2_vec_chunks;

    // private
    int32_t cur_a1_vec_chunk_id;
    int32_t cur_a2_vec_chunk_id;
    uint32_t num_padding_vecs;
    bool is_open;
    int fd;
    FILE *fp;
    off_t file_size;
    uint8_t *map;
    off_t cur_map_byte;
    off_t start_map_byte;
    bool is_little_endian;
    struct profile *profile;
    struct dots *dots;
};


/*
 * Prototypes
 *
 */

// public

// load an rbt, bin, bit or stim. Actual dots file not supported yet.
struct stim *get_stim_by_path(const char *profile_path, const char *path);

// Load a dots object. Must be fully populated with vectors but not expanded. 
struct stim *get_stim_by_dots(struct profile *profile, struct dots *dots);

// Load and fills the next chunk. Always unloads current chunk. 
struct vec_chunk *stim_load_next_chunk(struct stim *stim, enum artix_selects artix_select);
enum stim_types get_stim_type_by_path(const char *path);

// Serialization of raw stim files
void stim_serialize_to_path(struct stim *stim, const char *path);
struct stim *stim_deserialize(struct stim *stim);
struct vec_chunk *stim_decompress_vec_chunk(struct vec_chunk *chunk);

// get enable_pins array for gvpu TEST_SETUP
uint8_t *stim_get_enable_pins_data(struct stim *stim, enum artix_selects artix_select);

// private
struct stim *init_stim(struct stim *stim, struct profile_pin **pins, 
    uint32_t num_pins, uint32_t num_vecs, uint64_t num_unrolled_vecs);

struct vec_chunk *stim_fill_chunk(struct stim *stim, struct vec_chunk *chunk);
struct vec_chunk *stim_fill_chunk_by_dots(struct stim *stim,
    struct vec_chunk *chunk, struct dots *dots, 
    void (*get_next_data_subvecs)(struct stim *, enum subvecs **, 
    uint32_t*));
void stim_unload_chunk(struct vec_chunk *chunk);


enum subvecs *convert_bitstream_word_to_subvecs(uint32_t *word, 
    uint32_t *num_subvecs);
uint32_t stim_get_next_bitstream_word(struct stim *stim);
uint32_t calc_num_padding_vecs(uint32_t num_vecs);

struct stim *create_stim(void);
struct vec_chunk **create_vec_chunks(uint32_t num_vec_chunks);
struct vec_chunk *create_vec_chunk(uint8_t vec_chunk_id, 
        enum artix_selects artix_select, uint32_t num_vecs);
struct vec *create_vec(void);
struct stim *free_stim(struct stim *stim);
struct vec_chunk *free_vec_chunk(struct vec_chunk *chunk);
struct vec *free_vec(struct vec *vec);

#ifdef __cplusplus
}
#endif
#endif
