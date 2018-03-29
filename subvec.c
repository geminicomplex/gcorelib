/*
 * Subvec
 *
 */

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "common.h"
#include "subvec.h"


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
void pack_subvecs_with_opcode_and_operand(uint8_t *packed_subvecs, enum subvec_opcode opcode, uint32_t operand){
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
enum subvecs get_subvec_by_pin_id(uint8_t *packed_subvecs, uint32_t pin_id){
    if(packed_subvecs == NULL){
        die("pointer is NULL\n");
    }
    if((pin_id+1) >= DUT_TOTAL_NUM_PINS){
        die("error: failed to pack vec by pin, pin_id+1 >= 400\n");
    }

    enum subvecs subvec = DUT_SUBVEC_NONE; 

    uint32_t subvec_id = pin_id/2;
    uint8_t packed_subvec = packed_subvecs[subvec_id];

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



