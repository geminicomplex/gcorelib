/*
 * dots & rbt/bit
 *
 */

#ifndef STIM_H
#define STIM_H

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

#include "profile.h"
#include "config.h"
#include "lib/avl/avl.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#define DUT_NUM_PINS (200)
#define DUT_TOTAL_NUM_PINS (400)
#define DUT_MAX_VECTORS (67108864)

// a vector is 128 bytes or 256 nibbles
// a subvec is stored in a nibble and we have 
// 200 subvecs for 200 pins
#define STIM_VEC_SIZE (128)

// 4 byte word, 1024 byte burst and 4096 byte page aligned
#define STIM_CHUNK_SIZE (268435456)

// 8 vecs per 1024 byte burst
#define STIM_NUM_VECS_PER_BURST (8)

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
 * A 4 bit subvec holds the stimulus, expect and result
 * for one pin. Result held in bit[3], stim/expect held
 * in bit[2:0].
 *
 */
enum subvecs {
    DUT_SUBVEC_NONE = 0xf,
    DUT_SUBVEC_0 = 0x0,
    DUT_SUBVEC_1 = 0x1,
    DUT_SUBVEC_X = 0x2,
    DUT_SUBVEC_H = 0x3,
    DUT_SUBVEC_L = 0x4,
    DUT_SUBVEC_C = 0x5
} __attribute__ ((__packed__));


/*
 * Each vec represents a variable length vector based on the 
 * number of pins, but not exceeding the max length of 400.
 * Each pin is represented by a subvec, and to conserve space
 * 4 bit subvecs are packed into a byte, thus needing half the
 * space. If odd number of pins, will add one to have enough.
 *
 *
 */
struct vec {
    uint8_t *packed_subvecs;
} __attribute__ ((__packed__));

/*
 * Each vector is 128 bytes but only 100 are used for the actual vector.
 * the rest of the 28 bytes are used for the opcode and it's operand.
 *
 */
enum vec_opcode {
    DUT_OPCODE_NOP      = 0xff,
    DUT_OPCODE_VEC      = 0x01,
    DUT_OPCODE_VECLOOP  = 0x02,
    DUT_OPCODE_VECCLK   = 0x03
} __attribute__ ((__packed__));

/*
 * vec_chunk holds a group of vecs, not exceeding STIM_CHUNK_SIZE
 * in total size. Chunks also get dynamically loaded and unloaded
 * as needed, with only one being active at any time. Source files
 * are parsed specific to it's type, and loaded into chunks using
 * mmap.
 *
 * id : id of the chunk
 * is_loaded : loaded in memory yet
 * num_vecs : number of vectors
 * vecs : vector array
 * cur_vec_id : when filling vecs, used to inc
 * size : chunk size in bytes, including vecs
 * repeats : avl tree holding a repeat value for a given vec if it has one
 * 
 *
 */
struct vec_chunk {
    uint8_t id;
    bool is_loaded;
    uint32_t num_vecs;
    uint32_t cur_vec_id;
    uint8_t *vec_data;
    size_t vec_data_size;
    size_t size;
};

/*
 * A stimulus object represents a dots, rbt, bin, bit or raw stim file.
 * It's comprised of vec_chunk objects, which get dynamically loaded and
 * unloaded as needed, for the purpose of not filling up memory.
 *
 */
struct stim {
    // public
    enum stim_types type;
    uint16_t num_pins;
    struct profile_pin **pins;
    bool pins_active[DUT_TOTAL_NUM_PINS];
    uint32_t num_vecs;
    uint32_t num_unrolled_vecs;
    uint32_t num_vec_chunks;
    struct vec_chunk **vec_chunks;

    // private
    int32_t cur_vec_chunk_id;
    uint32_t num_padding_vecs;
    bool is_open;
    int fd;
    FILE *fp;
    off_t file_size;
    uint8_t *map;
    off_t cur_map_byte;
    bool is_little_endian;
};


/*
 * Prototypes
 *
 */

// public
struct stim *open_stim(const char *profile_path, const char *path);
struct vec_chunk *stim_load_next_chunk(struct stim *stim);
enum subvecs get_subvec_by_pin_id(struct vec *vec, uint32_t pin_id);
enum stim_types get_stim_type_by_path(const char *path);

// private
struct stim *init_stim(struct stim *stim, struct profile_pin **pins, 
    uint32_t num_pins, uint32_t num_vecs, uint32_t num_unrolled_vecs);
void close_stim(int status, void *vstim);

struct vec_chunk *stim_fill_chunk(struct stim *stim, struct vec_chunk *chunk);
struct vec_chunk *stim_fill_chunk_by_dots(struct stim *stim,
        struct vec_chunk *chunk, struct dots *dots, 
        uint32_t num_dots_vecs_to_load,
        void (*get_next_data_subvecs)(struct stim *, enum subvecs **, uint32_t*));
void stim_unload_chunk(struct vec_chunk *chunk);

void pack_subvecs_by_pin_id(uint8_t *packed_subvecs, uint32_t pin_id, enum subvecs subvec);
void pack_subvecs_by_dut_io_id(uint8_t *packed_subvecs, uint32_t dut_io_id, enum subvecs subvec);
enum subvecs *convert_bitstream_word_to_subvecs(uint32_t *word, 
    uint32_t *num_subvecs);
uint32_t stim_get_next_bitstream_word(struct stim *stim);

struct stim *create_stim(void);
struct vec_chunk *create_vec_chunk(uint8_t vec_chunk_id, uint32_t num_vecs);
struct vec *create_vec(void);
struct stim *free_stim(struct stim *stim);
struct vec_chunk *free_vec_chunk(struct vec_chunk *chunk);
struct vec *free_vec(struct vec *vec);

#ifdef __cplusplus
}
#endif
#endif
