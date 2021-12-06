/*
 * Subvec
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef SUBVEC_H
#define SUBVEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * A 4 bit subvec holds the stimulus and expect for one pin. 
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
 * Each vector is 128 bytes but only 100 are used for the actual vector.
 * the rest of the 28 bytes are used for the opcode and it's operand.
 *
 */
enum subvec_opcode {
    DUT_OPCODE_NOP      = 0xff,
    DUT_OPCODE_VEC      = 0x01,
    DUT_OPCODE_VECLOOP  = 0x02,
    DUT_OPCODE_VECCLK   = 0x03
} __attribute__ ((__packed__));


enum subvecs get_subvec_by_pin_id(uint8_t *packed_subvecs, 
    uint32_t pin_id);
void pack_subvecs_by_pin_id(uint8_t *packed_subvecs, 
    uint32_t pin_id, enum subvecs subvec);
void pack_subvecs_by_dut_io_id(uint8_t *packed_subvecs, 
    uint32_t dut_io_id, enum subvecs subvec);
void pack_subvecs_with_opcode_and_operand(uint8_t *packed_subvecs, 
    enum subvec_opcode opcode, uint64_t operand);


#ifdef __cplusplus
}
#endif
#endif
