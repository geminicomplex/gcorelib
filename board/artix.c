/*
 * Artix
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

/*
 * Methods to interact with the artix units.
 *   - configure units
 *   - write raw data to artix memory
 *   - read raw data to artix memory
 *   - run memory test
 *   - run dut test
 *
 */

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
#include <signal.h>
#include <time.h>

#include "../common.h"
#include "../util.h"
#include "../stim.h"
#include "../profile.h"
#include "artix.h"
#include "helper.h"
#include "dma.h"
#include "subcore.h"
#include "driver.h"


static void subcore_prep_dma_write(enum artix_selects artix_select, uint32_t num_bursts){
    struct gcore_ctrl_packet packet;

    subcore_idle();

    // pass number of bursts to subcore
    slog_debug(0, "subcore bursts: %d @ %d = %d bytes", BURST_BYTES, 
        num_bursts, (num_bursts*BURST_BYTES));
    helper_subcore_load(artix_select, SETUP_BURST);
    
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = num_bursts;
    subcore_write_packet(&packet);
    
    subcore_idle();

    slog_debug(0, "subcore: dma_write");
    helper_subcore_load(artix_select, DMA_WRITE);

    return;
}

void artix_mem_write(enum artix_selects artix_select,
        uint64_t addr, uint64_t *write_data, size_t write_size){
    uint64_t *dma_buf;

    // address can't be greater than artix memory capacity
    // and you need to send at least one burst (1024 bytes)
    if(addr > (uint64_t)(0x1ffffffff-BURST_BYTES)){
        die("error: address given is greater than (0x1ffffffff-1024)");
    }

    // can't allocate more than 250MB in one write chunk
    if(write_size > MAX_CHUNK_SIZE){
        die("error: write size %zu is greater than"
            "the max size we can allocate %zu.", write_size, (size_t)MAX_CHUNK_SIZE);
    }

    // each burst is 1024 bytes
    uint32_t num_bursts = (write_size / BURST_BYTES);

    // extra data so add one more burst
    if((write_size % BURST_BYTES) != 0){
        num_bursts = num_bursts + 1;
    }

    // setup memcore with start_addr and num_bursts will load
    helper_burst_setup(artix_select, addr, num_bursts);

    // debug status
    helper_print_agent_status(artix_select);

    // place memcore into write burst mode
    helper_memcore_load(artix_select, MEMCORE_WRITE_BURST);
    helper_memcore_check_state(artix_select, MEMCORE_WRITE_BURST, num_bursts);
    
    // debug status
    helper_print_agent_status(artix_select);

    // config gvpu to proxy data
    helper_gvpu_load(artix_select, MEM_WRITE);

    // debug status
    helper_print_agent_status(artix_select);

    // config agent to proxy data
    helper_agent_load(artix_select,  GVPU_WRITE);

    // reset dma buffer
    gcore_dma_alloc_reset();

    // config num bursts and config subcore to proxy data
    subcore_prep_dma_write(artix_select, num_bursts);

    if(write_size > DMA_SIZE){
        dma_buf = (uint64_t *)gcore_dma_alloc(DMA_SIZE, sizeof(uint8_t));
        memset(dma_buf, 0, DMA_SIZE);

        uint32_t num_chunks = (write_size/DMA_SIZE);

        // send chunks of max buffer size
        for(int i=0; i<num_chunks; i++){
            slog_info(0,"writing %i bytes...", DMA_SIZE);
            memset(dma_buf, 0, DMA_SIZE);
            memcpy(dma_buf, ((uint8_t*)write_data)+(i*DMA_SIZE), DMA_SIZE);
            gcore_dma_prep(dma_buf, DMA_SIZE, NULL, 0);
            gcore_dma_start(GCORE_WAIT_TX);
        }

        // send rest of data
        size_t data_mod = write_size % DMA_SIZE;
        if(data_mod != 0){
            slog_info(0,"writing %zu bytes...", data_mod);
            memcpy(dma_buf, ((uint8_t*)write_data)+(write_size-data_mod), data_mod);
            gcore_dma_prep(dma_buf, data_mod, NULL, 0);
            gcore_dma_start(GCORE_WAIT_TX);
        }

        // get burst count from gvpu
        uint32_t gvpu_burst_count = helper_get_agent_gvpu_status(artix_select,
                GVPU_STATUS_SELECT_MEM_RW,
                GVPU_STATUS_CMD_GET_CYCLE) + 1;
        slog_info(0,"sent %zu total bytes (actual %i).", 
            (num_chunks*DMA_SIZE)+data_mod, 
            gvpu_burst_count*BURST_BYTES);

    }else{
        // this always ensures that what we read is a multiple of a burst
        // since num_bursts is calculated based on write_size
        size_t size = num_bursts*BURST_BYTES; 
        slog_info(0,"writing %zu bytes (actual %zu)...", write_size, size);
        dma_buf = (uint64_t *)gcore_dma_alloc(size, sizeof(uint8_t));
        memset(dma_buf, 0, size);
        memcpy(dma_buf, write_data, write_size);
        gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, size, NULL, 0);
        
        // get burst count from gvpu
        uint32_t gvpu_burst_count = helper_get_agent_gvpu_status(artix_select,
                GVPU_STATUS_SELECT_MEM_RW,
                GVPU_STATUS_CMD_GET_CYCLE) + 1;
        slog_info(0,"sent %zu bytes (actual %i).", 
            write_size, gvpu_burst_count*BURST_BYTES);

    }

//#ifdef GEM_DEBUG
//    for(int i=0; i<32;i++){
//        slog_debug(0,"dma_buf %02i: 0x%016" PRIX64 "", i, write_data[i]);
//    }
//#endif

    // subcore must be idle
    subcore_idle();

    // reset burst count
    helper_gvpu_load(artix_select, TEST_CLEANUP);

    // debug status
    helper_print_agent_status(artix_select);
    return;
}

// subcore must assert last on the last burst of the last beat and a dma
// descriptor transfer can't be more than 2^23-1 due to axi dma ip
// limitation. If transfering more, than 8MB just keep calling this. For
// agent, gvpu and memcore you can just set max num_bursts.
// TODO: modify subcore to automatically send last
// if it's num_bursts > 65504
static void subcore_prep_dma_read(enum artix_selects artix_select, uint32_t num_bursts){
    struct gcore_ctrl_packet packet;

    // (2^23 - (4096 linux page) / 128) = 65504
    if(num_bursts > 65504){
        die("gcore: subcore_prep_dma_read error, can't set num bursts: %i > 65535", num_bursts);
    }

    slog_debug(0, "subcore bursts: %d @ %d = %d bytes", BURST_BYTES, 
        num_bursts, (num_bursts*BURST_BYTES));

    subcore_idle();

    // kernel sends bursts of 128 bytes to subcore. However
    // artix core operates with bursts of 1024 bytes so the
    // number of bursts will be different.
    helper_subcore_load(artix_select, SETUP_BURST);

    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = num_bursts;
    subcore_write_packet(&packet);

    subcore_idle();

    slog_debug(0, "subcore: dma_read");
    helper_subcore_load(artix_select, DMA_READ);
    return;
}

void artix_mem_read(enum artix_selects artix_select, uint64_t addr,
        uint64_t *read_data, size_t read_size){
    uint64_t *dma_buf;

    // 
    if(read_size > MAX_CHUNK_SIZE){
        die("error: read size %zu is greater than"
            "the max size we can allocate %zu.", read_size, (size_t)MAX_CHUNK_SIZE);
    }

    // kernel sends bursts of 128 bytes to subcore.
    uint32_t num_bursts = read_size / BURST_BYTES;

    // extra data so add one more burst
    if((read_size % BURST_BYTES) != 0){
        num_bursts = num_bursts + 1;
    }

    // setup memcore with start_addr and num_bursts will read
    helper_burst_setup(artix_select, addr, num_bursts);

    // debug status
    helper_print_agent_status(artix_select);

    // fill packet
    helper_memcore_load(artix_select, MEMCORE_READ_BURST);
    helper_memcore_check_state(artix_select, MEMCORE_READ_BURST, num_bursts);

    helper_print_agent_status(artix_select);

    helper_gvpu_load(artix_select, MEM_READ);

    helper_agent_load(artix_select, GVPU_READ);

    gcore_dma_alloc_reset();

    if(read_size > DMA_SIZE){
        // allocate largest possible dma buffer
        dma_buf = (uint64_t *)gcore_dma_alloc(DMA_SIZE, sizeof(uint8_t));
        memset(dma_buf, 0, DMA_SIZE);

        uint32_t num_chunks = (read_size/DMA_SIZE);

        // receive chunks of DMA_SIZE
        for(int i=0; i<num_chunks;i++){
            slog_info(0,"reading %i bytes...", DMA_SIZE);
            memset(dma_buf, 0, DMA_SIZE);
            gcore_dma_prep(NULL, 0, dma_buf, DMA_SIZE);
            subcore_prep_dma_read(artix_select, DMA_SIZE/BURST_BYTES);
            gcore_dma_start(GCORE_WAIT_RX);
            memcpy(((uint8_t*)read_data)+(i*DMA_SIZE), dma_buf, DMA_SIZE);
        }

        // send the rest
        size_t data_mod = read_size % DMA_SIZE;
        if(data_mod != 0){
            slog_info(0,"reading %zu bytes...", data_mod);
            gcore_dma_prep(NULL, 0, dma_buf, data_mod);
            subcore_prep_dma_read(artix_select, data_mod/BURST_BYTES);
            gcore_dma_start(GCORE_WAIT_RX);
            memcpy(((uint8_t*)read_data)+(read_size-data_mod), dma_buf, data_mod);
        }

        // get burst count from gvpu
        uint32_t gvpu_burst_count = helper_get_agent_gvpu_status(artix_select,
                GVPU_STATUS_SELECT_MEM_RW,
                GVPU_STATUS_CMD_GET_CYCLE) + 1;
        slog_info(0,"received %zu total bytes (actual %i).", 
            (num_chunks*DMA_SIZE)+data_mod, 
            gvpu_burst_count*BURST_BYTES);
    }else{
        // this always ensures that what we read is a multiple of a burst
        // since num_bursts is calculated based on read_size
        size_t size = num_bursts*BURST_BYTES; 
        slog_info(0,"reading %zu bytes (actual %zu)...", read_size, size);
        dma_buf = (uint64_t *)gcore_dma_alloc(size, sizeof(uint8_t));
        memset(dma_buf, 0, size);
        gcore_dma_prep(NULL, 0, dma_buf, size);
        subcore_prep_dma_read(artix_select, num_bursts);
        gcore_dma_start(GCORE_WAIT_RX);
        memcpy(read_data, dma_buf, read_size);

        // get burst count from gvpu
        uint32_t gvpu_burst_count = helper_get_agent_gvpu_status(artix_select,
                GVPU_STATUS_SELECT_MEM_RW,
                GVPU_STATUS_CMD_GET_CYCLE) + 1;
        slog_info(0,"received %zu bytes (actual %i).", 
            read_size, gvpu_burst_count*BURST_BYTES);
    }

//#ifdef GEM_DEBUG
//    for(int i=0; i<32;i++){
//        slog_debug(0,"dma_buf %02i: 0x%016" PRIX64 "", i, read_data[i]);
//    }
//#endif

    // wait for idle state
    subcore_idle();

    // reset burst count
    helper_gvpu_load(artix_select, TEST_CLEANUP);

    helper_print_agent_status(artix_select);
    return;
}

// from: https://stackoverflow.com/questions/33010010/how-to-generate-random-64-bit-unsigned-integer-in-c
#define IMAX_BITS(m) ((m)/((m)%255+1) / 255%255*8 + 7-86/((m)%255+12))
#define RAND_MAX_WIDTH IMAX_BITS(RAND_MAX)
#ifndef VERILATOR
_Static_assert((RAND_MAX & (RAND_MAX + 1u)) == 0, "RAND_MAX not a Mersenne number");
#endif
uint64_t rand64(void) {
  uint64_t r = 0;
  for (int i = 0; i < 64; i += RAND_MAX_WIDTH) {
    r <<= RAND_MAX_WIDTH;
    r ^= (unsigned) rand();
  }
  return r;
}

/*
 * Returns one burst with random data for beats 0 to 5. Calc custom crc for beats
 * 6 and 7.
 *
 */
__attribute__((optimize("-O2"))) static uint64_t* get_mem_test_burst(uint32_t seed){
    uint64_t num = 0;
    uint64_t *burst = NULL;

    // 1024 bytes per burst, or 128 64-bit words
    if((burst=(uint64_t*)calloc(128, sizeof(uint64_t))) == NULL){
        die("calloc failed");
    }

    srand(seed);

    uint64_t bxor = 0xffffffffffffffff;

    for(uint32_t i=0; i<128; i++){
        // copy crc for last two beat
        if(i>=96){
            memcpy((uint8_t*)burst+(i*sizeof(uint64_t)), &bxor, sizeof(uint64_t));
            continue;
        }
        num = rand64();

        if(i%16==0){
            bxor = bxor ^ bxor;
        }

        for(int32_t j=0; j<sizeof(int64_t); j++){
            bxor = bxor ^ *(((uint8_t*)&num)+j);
        }

        if((i+1)%16==0){
            bxor = bxor << 8;
        }

        memcpy((uint8_t*)burst+(i*sizeof(uint64_t)), &num, sizeof(uint64_t));
    }

    return burst;
}

/*
 * Generates bursts with pseudo random data for 6 beats. If including crc data,
 * will calculate crc algo for beat 7. If clear crc results is true, beat 8
 * will zeroed out, otherwise it will also have the crc calculation.
 *
 * Start burst is available so we can generate test data in chunks and so we
 * can start off from where we left off. Start burst only affects the seed
 * value.
 *
 */
static uint64_t* get_mem_test_data(uint64_t start_burst, uint64_t num_bursts, bool include_crc_data, bool clear_crc_results){
    uint64_t num = 0;
    uint64_t *data = NULL;
    uint64_t *burst = NULL;

    if((data=(uint64_t*)calloc(num_bursts, BURST_BYTES)) == NULL){
        die("calloc failed for num bytes %" PRId64 "", num_bursts*BURST_BYTES);
    }

    srand(1);

    slog_info(0, "generating mem test data for bursts %" PRId64 " to %" PRId64  "...", 
            start_burst, start_burst+num_bursts);
    for(uint64_t i=start_burst; i<start_burst+num_bursts; i++){
        burst = get_mem_test_burst(i+1);
        uint32_t idx = 0;
        for(uint32_t j=0; j<NUM_BEATS_PER_BURST; j++){
            for(uint32_t k=0; k<NUM_WORDS_PER_BEAT; k++){
                idx = (j*NUM_WORDS_PER_BEAT)+k;
                if(include_crc_data && (j==6 || j==7)){
                    if(j==6){
                        num = burst[idx];
                    }else if(j==7){
                        if(clear_crc_results){
                            num = 0x0000000000000000;
                        }else{
                            num = burst[idx];
                        }
                    }
                } else {
                    num = burst[idx];
                }
                data[((i-start_burst)*128)+idx] = num;
            }
        }
        free(burst);
    }
    slog_info(0, "mem test data generated.");
    return data;
}

/*
 * Partially prints a dma burst for debugging purposes.
 *
 */
void static debug_print_dma_burst(bool print_end, uint64_t *data, size_t data_size){
    uint32_t start_beat = 0;
    uint32_t end_beat = 0;

    // each burst is 1024 bytes
    uint32_t num_bursts = (data_size / BURST_BYTES);

    // print only max three beats from burst
    if(num_bursts > 1){
        if(print_end) {
            start_beat = 16*5;
            end_beat = 16*8;
            slog_debug(0, "printing write beats 5:7");
        } else {
            slog_debug(0, "printing write beats 0:2");
            start_beat = 16*0;
            end_beat = 16*2;
        }
    }else{
        start_beat = 0;
        end_beat = (data_size/sizeof(uint64_t));
    }
    for(int i=start_beat; i<end_beat;i++){
        if(i%16 == 0){
            slog_debug(0, "--------------------------------------------------");
        }
        slog_debug(0, "dma_buf %02i: 0x%016" PRIX64 "", i, data[i]);
    }
}

/*
 * Loads artix memory with mem test data up to num_chunks.
 *
 */
void static artix_load_mem_test_data(enum artix_selects artix_select, bool run_crc, uint64_t num_chunks, uint64_t chunk_size){
    if((chunk_size % BURST_BYTES) != 0){
        die("mem test data size: %d bytes is not burst page aligned to %d bytes\n", chunk_size, BURST_BYTES);
    }

    // always start the test at address zero
    uint64_t addr = 0x0000000000000000;
    uint64_t start_burst = 0;
    uint64_t num_bursts = (chunk_size / BURST_BYTES);
    uint64_t *chunk = NULL;

    for(uint32_t i=0; i<num_chunks; i++){
        if(run_crc){
            // include crc data and clear last beat in burst
            chunk = get_mem_test_data(start_burst, num_bursts, true, true);
        }else if(!run_crc){
            // include crc data and write to last beat as well
            chunk = get_mem_test_data(start_burst, num_bursts, true, false);
        }
        start_burst += num_bursts;

#ifdef GEM_DEBUG
        if(i == 0){
            debug_print_dma_burst(run_crc, chunk, chunk_size);
        }
#endif

        slog_info(0, "writing mem data chunk %" PRId64 " of %" PRId64 " to rank:0x%X addr:0x%08X...", 
            i+1, num_chunks, GET_START_RANK(addr), GET_START_ADDR(addr));
        artix_mem_write(artix_select, addr, chunk, chunk_size);

        addr += chunk_size;

        // debug status
        helper_print_agent_status(artix_select);

        free(chunk);
    }

    return;
}

struct mem_test_check {
    int32_t first_fail_chunk;
    uint64_t first_fail_word;
    uint64_t total_fail_words;
};

struct mem_test_check static artix_check_mem_test_data(enum artix_selects artix_select, bool run_crc, uint64_t num_chunks, uint64_t chunk_size){
    uint64_t *write_data = NULL;
    uint64_t *read_data = NULL;
    uint64_t addr = 0x0000000000000000;
    uint64_t start_burst = 0;
    uint64_t num_bursts = (chunk_size / BURST_BYTES);
    uint64_t num_chunk_fail_words = 0;
    bool chunk_did_fail = false;
    struct mem_test_check test_check;

    test_check.first_fail_chunk = -1;
    test_check.first_fail_word = 0;
    test_check.total_fail_words = 0;

    // malloc read buffer
    if((read_data = (uint64_t *)calloc(chunk_size, sizeof(uint8_t))) == NULL){
        die("error: calloc failed");
    }


    for(uint32_t i=0; i<num_chunks; i++){
        artix_mem_read(artix_select, addr, read_data, chunk_size);

#ifdef GEM_DEBUG
        debug_print_dma_burst(run_crc, read_data, chunk_size);
#endif

        // re-generate write data with results not cleared out so 
        // it will match read_data
        write_data = get_mem_test_data(start_burst, num_bursts, true, false);
        start_burst += num_bursts;

        slog_info(0, "checking write/read array difference for chunk %i...", i);
        num_chunk_fail_words = 0;
        chunk_did_fail = false;
        for(int j=0; j<(chunk_size/sizeof(uint64_t)); j++){
            if(write_data[j] != read_data[j]){
                chunk_did_fail = true;
                test_check.first_fail_chunk = i;
                if(!test_check.first_fail_word){
                    test_check.first_fail_word = (i*(chunk_size/sizeof(uint64_t)))+j;
                }
                num_chunk_fail_words = num_chunk_fail_words + 1;
            }
        }

        // print the first failing chunk 
        if(chunk_did_fail && test_check.total_fail_words == 0){
            for(int j=0; j<(chunk_size/sizeof(uint64_t)); j++){
                if(j%16 == 0){
                    slog_debug(0, "--------------------------------------------------");
                }
                if(write_data[j] != read_data[j]){
                    slog_debug(0, "diff %02i:*0x%016" PRIX64 " 0x%016" PRIX64 "", j, write_data[j], read_data[j]);
                }else{
                    slog_debug(0, "diff %02i: 0x%016" PRIX64 " 0x%016" PRIX64 "", j, write_data[j], read_data[j]);
                }
            }
        }
        test_check.total_fail_words += num_chunk_fail_words;

        addr += chunk_size;

        free(write_data);
        slog_info(0, "check for chunk %i finished.", i);
    }

    free(read_data);

    return test_check;
}

/*
 * Runs an artix memory test. If run crc is true, will calculate crc value for burst,
 * compare to beat 7 andwrite result to beat 8. If beat 7 and 8 don't match, the test
 * will fail.
 *
 * If full test is true, will run the full 8GiB test. Otherwise, it will run a partial
 * test of MAX_CHUNK_SIZE.
 *
 */
bool artix_mem_test(enum artix_selects artix_select, bool run_crc, bool full_test){
    struct gcore_ctrl_packet packet;
    bool crc_failed = false;
    uint64_t crc_cycle = 0;
    uint64_t chunk_size = 0;
    uint64_t num_chunks = 0;
    uint64_t start_addr = 0x0000000000000000;
    uint64_t num_bursts = 0;
    struct gcore_registers *regs = NULL;
    enum gvpu_states gvpu_state = 0; 
    time_t test_start_time;
    time_t test_end_time;
    time_t check_start_time;
    time_t check_cur_time;
    double diff_time_secs = 0;
    bool did_test_pass = false;

#ifdef VERILATOR
    chunk_size = 1024*5; 
#else
    chunk_size = MAX_CHUNK_SIZE;
#endif

    if((chunk_size % BURST_BYTES) != 0){
        die("mem test data size: %d bytes is not burst page aligned to %d bytes\n", chunk_size, BURST_BYTES);
    }

    if(full_test){
#ifdef VERILATOR
        num_chunks = 2;
#else
        if(ARTIX_MEM_BYTES % MAX_CHUNK_SIZE != 0){
            die("chunk size %d is not aligned to artix mem bytes %d\n", MAX_CHUNK_SIZE, ARTIX_MEM_BYTES);
        }
        num_chunks = ARTIX_MEM_BYTES/MAX_CHUNK_SIZE;
#endif
    }else{
        num_chunks = 1;
    }

    // load mem test data in chunks to artix memory
    artix_load_mem_test_data(artix_select, run_crc, num_chunks, chunk_size);

    // performs crc on [0:5], check against 6th, and write to 7th
    // beat in each burst
    if(run_crc){

        // number of bursts in the test
        num_bursts = ((num_chunks*chunk_size) / BURST_BYTES);

        slog_info(0, "setup crc test with %" PRId64  " bursts starting at addr:0x%X%08X...",
            num_bursts, GET_START_RANK(start_addr), GET_START_ADDR(start_addr));
        // this sets gvpu_num_bursts reg since memtest looks at this
        helper_burst_setup(artix_select, start_addr, num_bursts);

        slog_info(0, "loading crc test...");
        helper_gvpu_load(artix_select, MEM_TEST);
        helper_print_agent_status(artix_select);

        slog_info(0, "running crc test...");
        uint64_t counter = 0;
        time(&test_start_time);
        time(&check_start_time);
        while(1){
            regs = subcore_get_regs();
            gvpu_state = get_gvpu_state(artix_select, regs);
            if(gvpu_state == GVPU_IDLE){
                time(&test_end_time);
                break;
            }else{
                crc_cycle = helper_get_agent_gvpu_status(artix_select,
                        GVPU_STATUS_SELECT_MEM_TEST,
                        GVPU_STATUS_CMD_GET_CYCLE);
                slog_info(0, "cycle %" PRId64 " of %" PRId64 "(%02f%%)", 
                        crc_cycle, num_bursts, ((double)crc_cycle/(double)num_bursts)*100.00);
                if(crc_cycle > counter){
                    time(&check_start_time);
                    counter = crc_cycle;
                }else{
                    time(&check_cur_time);
                    diff_time_secs = difftime(check_cur_time, check_start_time);
                    if(diff_time_secs >= 10){
                        die("mem test stuck on cycle %" PRId64 ". Please restart board and contact support.", crc_cycle);
                    }
                }
                if(crc_cycle == num_bursts){
                    time(&test_end_time);
                    break;
                }
            }
        }

        // grab number of test cycles and if it failed
        crc_cycle = helper_get_agent_gvpu_status(artix_select,
                GVPU_STATUS_SELECT_MEM_TEST,
                GVPU_STATUS_CMD_GET_CYCLE);
        slog_info(0, "cycle %" PRId64 " of %" PRId64 "(%" PRId64 "%%)", 
                crc_cycle, num_bursts, (crc_cycle/num_bursts)*100);
        helper_get_agent_status(artix_select, &packet);
        if((packet.data & 0x00010000) == 0x00010000){
            crc_failed = true;
        }
        slog_info(0, "crc test finished in %02f seconds.", 
            difftime(test_end_time, test_start_time));
    }

    helper_print_agent_status(artix_select);

    // reset cycle count and failed flag
    helper_gvpu_load(artix_select, TEST_CLEANUP);

    helper_print_agent_status(artix_select);

    /*
     * check_mem_test must hold both a read and write buffer in memory, which
     * can't be done if the chunk size is MAX_CHUNK_SIZE (500MB).
     *
     */
    if(chunk_size == MAX_CHUNK_SIZE){
        chunk_size = chunk_size / 2;
        num_chunks *= 2;
    }

    struct mem_test_check test_check = artix_check_mem_test_data(artix_select, run_crc, num_chunks, chunk_size);

    if(run_crc){
        if(crc_failed){
            slog_info(0, "mem test failed at word %" PRId64 " (ran %" PRId64 " cycles)", 
                    ((crc_cycle*1024)/8), crc_cycle);
        }else{
            slog_info(0, "mem test PASS (ran %" PRId64 " cycles)!", crc_cycle);
        }
    }

    if(test_check.total_fail_words == 0){
        slog_info(0, "mem data compare PASS!");
    }else{
        slog_info(0, "mem data compare FAIL :(");
        slog_info(0, "found %" PRId64 "failures in chunks starting at %i", test_check.total_fail_words, test_check.first_fail_word);
    }

    if(run_crc){
        if(crc_failed || test_check.total_fail_words > 0){
            did_test_pass = false;
        }else{
            did_test_pass = true;
        }
    }else{
        if(test_check.total_fail_words > 0){
            did_test_pass = false;
        }else{
            did_test_pass = true;
        }
    }

    slog_info(0, "mem test done.");
    

    return did_test_pass;
}

/*
 * Given a stim and an artix select, check to see if the
 * pin's dut_io_ids are within range for given artix unit.
 *
 */
static void assert_dut_io_range(struct stim *stim, enum artix_selects artix_select) {
    struct profile_pin *pin = NULL;
    uint32_t range_low = 0;
    uint32_t range_high = 0;

    if(stim == NULL){
        die("error: pointer is NULL");
    }

    // check for correct dut_io_id based on artix unit 
    if(artix_select == ARTIX_SELECT_A1){
        range_low = 0;
        range_high = DUT_NUM_PINS-1;
    }else if(artix_select == ARTIX_SELECT_A2){
        range_low = DUT_NUM_PINS;
        range_high = DUT_TOTAL_NUM_PINS-1;
    }

    for(int pin_id=0; pin_id<stim->num_pins; pin_id++){
        pin = stim->pins[pin_id];
        if(pin->dut_io_id < range_low || pin->dut_io_id > range_high){
            die("error: dut_io_id %i is out of range for artix unit", pin->dut_io_id);
        }
    }
    return;
}

/*
 * Load a stim into tester memory at an arbitrary address. Be careful not to
 * clobber other patterns.
 *
 * Technically if it's a dual pattern, you can load it at different addrs, but
 * for simplicity, always load a1 and a2 at the same address.
 *
 * Returns the next available address.
 */
uint64_t artix_load_stim(struct stim *stim, uint64_t load_addr){
    struct vec_chunk *chunk;
    if(stim == NULL){
        die("pointer is null");
    }
    // 2**33 = 8589934592 or 8GB of mem
    // 8GB / 8 = 0x40000000
    // 0x40000000-1 because addr starts at zero
    if(load_addr > (0x40000000-1)){
        bye("failed to load stim at addr 0x%016" PRIX64 " because out of tester memory range", load_addr);
    }

    if(load_addr % BURST_BYTES != 0){
        bye("failed to load stim at addr 0x%016" PRIX64 " because it is not memory aligned to 1024 bytes", load_addr);
    }

    uint64_t addr = 0x0;
    enum artix_selects artix_select = ARTIX_SELECT_NONE;

    for(int i=0; i<2; i++){
        if(stim_get_mode(stim) == STIM_MODE_DUAL){
            if(i == 0){
                artix_select = ARTIX_SELECT_A1;
            }else if(i == 1){
                artix_select = ARTIX_SELECT_A2;
            }else{
                continue;
            }
        }else if(stim_get_mode(stim) == STIM_MODE_A1){
            if(i == 0){
                artix_select = ARTIX_SELECT_A1;
            }else{
                continue;
            }
        }else if(stim_get_mode(stim) == STIM_MODE_A2){
            if(i == 1){
                artix_select = ARTIX_SELECT_A2;
            }else{
                continue;
            }
        }else{
            continue;
        }
        addr = load_addr;

        slog_info(0, "writing vectors to memory...");
        // load one chunk at a time and dma the vecs to artix memory
        while((chunk = stim_load_next_chunk(stim, artix_select)) != NULL){

            // copy over the vec data buffer
            slog_info(0, "writing %i vecs (%zu bytes) to artix memory at address 0x%016" PRIX64 "...", 
                chunk->num_vecs, chunk->vec_data_size, addr);
            artix_mem_write(artix_select, load_addr, (uint64_t*)(chunk->vec_data), chunk->vec_data_size);

            // update the address pointer based on how much we copied in bytes
            addr += (uint64_t)chunk->vec_data_size;
        }

        // reset test_cycle counter and test_failed flag
        helper_gvpu_load(artix_select, TEST_CLEANUP);
    }

    // return next available address both are loaded into same address so return
    // will be same for both a1 and a2 if dual mode
    return addr;
}

/*
 * Preps the gvpu to execute a stim dut test. Must be called before every dut test.
 *  + sets the test start addr
 *  + sets total num vecs to execute
 *  + sets which pins are enabled for test
 *
 */
static void artix_setup_stim(struct stim *stim, enum artix_selects artix_select, 
        uint64_t start_addr){
    struct gcore_ctrl_packet packet;
    uint64_t *dma_buf;
    uint32_t num_bursts;

    if(stim == NULL){
        die("pointer is NULL");
    }

    // check if dut_io_id is within range for given artix
    //assert_dut_io_range(stim, artix_select);

    // perform test init
    helper_gvpu_load(artix_select, TEST_INIT);
    packet.rank_select = GET_START_RANK(start_addr);
    packet.addr = GET_START_ADDR(start_addr);
    packet.data = (stim->num_vecs+stim->num_padding_vecs);
    helper_gvpu_packet_write(artix_select, &packet);

    // perform test setup by writing enable_pins burst
    helper_gvpu_load(artix_select, TEST_SETUP);
    helper_agent_load(artix_select, GVPU_WRITE);

    // reset dma buffer
    gcore_dma_alloc_reset();

    // send one burst of data (1024 bytes)
    num_bursts = 1;
    subcore_prep_dma_write(artix_select, num_bursts);

    size_t burst_size = num_bursts*BURST_BYTES;
    slog_info(0, "sending setup burst (%i bytes)...", burst_size);

    uint8_t *enable_pins = NULL;
    if((enable_pins = stim_get_enable_pins_data(stim, artix_select)) == NULL){
        die("failed to alloc enable_pins");
    }

    // Note: no need to swap the endianess of enable_pins because of the
    // way 64 bit words are packed in agent, gvpu and memcore

    // write the enable pins to TEST_SETUP
    dma_buf = (uint64_t *)gcore_dma_alloc(burst_size, sizeof(uint8_t));
    memset(dma_buf, 0xffffffff, burst_size);
    memcpy(dma_buf, enable_pins, burst_size);

#ifdef GEM_DEBUG
    for(int i=0; i<32;i++){
        slog_debug(0, "pin_enable %02i: 0x%016" PRIX64 "", i, dma_buf[i]);
    }
#endif

    gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, burst_size, NULL, 0);
    free(enable_pins);

    // reset cycle count and failed flag
    helper_gvpu_load(artix_select, TEST_CLEANUP);

    return;
}

/*
 * Queries both A1 and A2 for fail pins. Returns an array with len 400.
 * If stim is solo pattern check A1 or A2. If stim is dual check entire array.
 *
 */
void artix_get_stim_fail_pins(uint8_t **fail_pins, uint32_t *num_fail_pins){
    uint64_t *dma_buf = NULL;
    uint32_t num_bursts = 1;
    size_t burst_size = num_bursts*BURST_BYTES;
    enum artix_selects artix_select = ARTIX_SELECT_NONE;

    // grab a1 and a2 fail pins
    (*fail_pins) = NULL;
    (*num_fail_pins) = 400;
    if(((*fail_pins) = (uint8_t*)calloc((*num_fail_pins), sizeof(uint8_t))) == NULL){
        die("error: calloc failed");
    }
    for(int i=0; i<(*num_fail_pins); i++){
        (*fail_pins)[i] = 0x00;
    }

    for(int select=0; select<2; select++){
        if(select == 0){
            artix_select = ARTIX_SELECT_A1;
        }else if(select == 1){
            artix_select = ARTIX_SELECT_A2;
        }else{
            continue;
        }

        helper_gvpu_load(artix_select, TEST_FAIL_PINS);
        helper_agent_load(artix_select, GVPU_READ);

        // reset dma buffer
        gcore_dma_alloc_reset();

        // send one burst of data (1024 bytes)
        subcore_prep_dma_read(artix_select, num_bursts);

        slog_info(0, "sending setup burst (%i bytes)...", burst_size);

        // write the enable pins to TEST_SETUP
        dma_buf = (uint64_t *)gcore_dma_alloc(burst_size, sizeof(uint8_t));
        memset(dma_buf, 0xffffffff, burst_size);

        gcore_dma_prep(NULL, 0, dma_buf, burst_size);
        //subcore_prep_dma_read(artix_select, burst_size);
        gcore_dma_start(GCORE_WAIT_RX);
        if(select == 0){
            for(int i=0; i<200; i++){
                if(((uint8_t *)dma_buf)[i]){
                    (*fail_pins)[i] = 1;
                }else{
                    (*fail_pins)[i] = 0;
                }
            }
        }else if(select == 1){
            for(int i=0; i<200; i++){
                if(((uint8_t *)dma_buf)[i]){
                    (*fail_pins)[i+200] = 1;
                }else{
                    (*fail_pins)[i+200] = 0;
                }
            }
        }

#ifdef GEM_DEBUG
        for(int i=0; i<32;i++){
            slog_debug(0, "fail_pin %02i: 0x%016" PRIX64 "", i, dma_buf[i]);
        }
#endif
    }

    return;
}

void artix_print_stim_fail_pins(struct stim *stim, uint8_t *fail_pins, uint32_t num_fail_pins){
    struct profile_pin *pin = NULL;

    if(stim == NULL){
        die("pointer is NULL");
    }
    if(fail_pins == NULL){
        die("pointer is NULL");
    }

    printf("   pins: ");

    for(int i=0; i<stim->num_pins; i++){
        pin = stim->pins[i];
        printf("%s ", pin->net_name);
    }
    printf("\n");

    printf("                ");
    for(int i=0; i<stim->num_pins; i++){
        pin = stim->pins[i];
        if(fail_pins[pin->dut_io_id]){
            printf("F ");
        }else{
            printf(". ");
        }
        for(int j=0; j<strlen(pin->net_name); j++){
            printf(" ");
        }
    }
    printf("\n");
}


//
// Execute the stim in tester memory at the addresses given.
//
// returns -1 if pass or failing cycle number (zero indexed)
//
//
bool artix_run_stim(struct stim *stim, uint64_t *test_cycle, uint64_t start_addr){
    struct gcore_ctrl_packet master_packet;
    struct gcore_ctrl_packet slave_packet;
    bool master_test_failed = false;
    bool slave_test_failed = false;
    uint64_t master_test_cycle = 0;
    uint64_t slave_test_cycle = 0;
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    uint64_t total_unrolled_vecs = 0;

    if(test_cycle != NULL){
        (*test_cycle) = 0;
    }

    if(stim == NULL){
        die("pointer is NULL");
    }

    if(stim->profile == NULL){
        die("no profile set for stim");
    }

    if(!stim->num_a1_vec_chunks && !stim->num_a2_vec_chunks){
        die("stim has neither a1 nor a2 chunks");
    }

    total_unrolled_vecs = (stim->num_unrolled_vecs+(uint64_t)(stim->num_padding_vecs));

    bool dual_mode = false;
    enum stim_modes stim_mode = stim_get_mode(stim);
    if(stim_mode == STIM_MODE_DUAL){
        dual_mode = true;
        artix_setup_stim(stim, ARTIX_SELECT_A1, start_addr);
        artix_setup_stim(stim, ARTIX_SELECT_A2, start_addr);

        // a1 is always the master in dual_mode
        artix_select = ARTIX_SELECT_A1;
    }else if(stim_mode == STIM_MODE_A1){
        artix_setup_stim(stim, ARTIX_SELECT_A1, start_addr);
        artix_select = ARTIX_SELECT_A1;
    }else if(stim_mode == STIM_MODE_A2){
        artix_setup_stim(stim, ARTIX_SELECT_A2, start_addr);
        artix_select = ARTIX_SELECT_A2;
    }else{
        bye("failed to execute stim with no vectors");
    }

    // setup a1 and a2 for dual mode
    if(dual_mode){
        subcore_artix_sync(true);
    }else{
        subcore_artix_sync(false);
    }

    helper_print_agent_status(artix_select);

    // run the test
    if(dual_mode){
        slog_info(0, "running test (dual mode)...");
        helper_gvpu_load(ARTIX_SELECT_A1, TEST_RUN);
        helper_gvpu_load(ARTIX_SELECT_A2, TEST_RUN);
    }else{
        slog_info(0, "running test...");
        helper_gvpu_load(artix_select, TEST_RUN);
        helper_print_agent_status(artix_select);
    }

    uint32_t counter = 0;
    while(1){
        helper_get_agent_status(artix_select, &master_packet);
        if((master_packet.data & 0x000000f0) != (TEST_RUN << 4)){
            if(dual_mode){
                helper_print_agent_status(ARTIX_SELECT_A1);
                helper_print_agent_status(ARTIX_SELECT_A2);
            }else{
                helper_print_agent_status(artix_select);
            }
            break;
        }else{
            if(counter >= 0x000fffff){
                master_test_cycle = helper_get_agent_gvpu_status(artix_select,
                        GVPU_STATUS_SELECT_DUT_TEST,
                        GVPU_STATUS_CMD_GET_CYCLE);
                if(dual_mode){
                    helper_print_agent_status(ARTIX_SELECT_A1);
                    helper_print_agent_status(ARTIX_SELECT_A2);
                }else{
                    helper_print_agent_status(artix_select);
                }
                counter = 0;
                break;
            }else{
                counter = counter + 1;
            }
        }
    }

    // grab number of test cycles and if it failed
    helper_get_agent_status(artix_select, &master_packet);
    if((master_packet.data & 0x00010000) == 0x00010000){
        master_test_failed = true;
    }

    master_test_cycle = helper_get_agent_gvpu_status(artix_select,
            GVPU_STATUS_SELECT_DUT_TEST,
            GVPU_STATUS_CMD_GET_CYCLE);

    if(dual_mode){
        helper_get_agent_status(ARTIX_SELECT_A2, &slave_packet);
        if((slave_packet.data & 0x00010000) == 0x00010000){
            slave_test_failed = true;
        }

        slave_test_cycle = helper_get_agent_gvpu_status(ARTIX_SELECT_A2,
            GVPU_STATUS_SELECT_DUT_TEST,
            GVPU_STATUS_CMD_GET_CYCLE);

        if((master_packet.addr & 0xf0000000) >> 30){
            slog_warn(0, "warning: a1 read fifo stalled during test");
        }

        // msb byte is 0:did_stall:status_switch
        if((slave_packet.addr & 0xf0000000) >> 30){
            slog_warn(0, "warning: a2 read fifo stalled during test");
        }

        if(test_cycle != NULL){
            if(master_test_cycle == slave_test_cycle){
                (*test_cycle) = master_test_cycle;
            }else if(master_test_cycle < slave_test_cycle){
                (*test_cycle) = master_test_cycle;
            }else if(slave_test_cycle < master_test_cycle){
                (*test_cycle) = slave_test_cycle;
            }
        }

        if(master_test_failed || slave_test_failed){
            if(master_test_cycle != slave_test_cycle){
                slog_fatal(0, "fail test cycle is not the same. a1:%llu a2:%llu", master_test_cycle, slave_test_cycle);
            }

            if(master_test_failed && slave_test_failed){
                slog_error(0, "test failed in a1 and a2 at vector %llu out of %llu :(", master_test_cycle, total_unrolled_vecs);
            }else if(master_test_failed){
                slog_error(0, "test failed in a1 at vector %llu out of %llu :(", master_test_cycle, total_unrolled_vecs);
            }else if(slave_test_failed){
                slog_error(0, "test failed in a2 at vector %llu out of %llu :(", slave_test_cycle, total_unrolled_vecs);
            }
        }else{
            if(master_test_cycle < total_unrolled_vecs || master_test_cycle > total_unrolled_vecs){
                slog_error(0, "a1 test failed (executed %llu of %llu vectors) (fail flag didn't assert)!", master_test_cycle, total_unrolled_vecs);
                master_test_failed = true;
            }
            if(slave_test_cycle < total_unrolled_vecs || slave_test_cycle > total_unrolled_vecs){
                slog_error(0, "a2 test failed (executed %llu of %llu vectors) (fail flag didn't assert)!", slave_test_cycle, total_unrolled_vecs);
                slave_test_failed = true;
            }

            if((master_test_failed && slave_test_failed) == false){
                if(master_test_cycle != slave_test_cycle){
                    slog_fatal(0, "test passed in a1 and a2 but the test cycle is not the same");
                }
                slog_info(0, "test PASS (executed %llu of %llu vectors)!", master_test_cycle, total_unrolled_vecs);
            }
        }

        if(master_test_failed || slave_test_failed){
            uint32_t num_fail_pins = 0;
            uint8_t *fail_pins = NULL;
            artix_get_stim_fail_pins(&fail_pins, &num_fail_pins);
            artix_print_stim_fail_pins(stim, fail_pins, num_fail_pins);
            free(fail_pins);
        }
    }else{

        master_test_cycle = helper_get_agent_gvpu_status(artix_select,
            GVPU_STATUS_SELECT_DUT_TEST,
            GVPU_STATUS_CMD_GET_CYCLE);


        // msb byte is 0:did_stall:status_switch
        if((master_packet.addr & 0xf0000000) >> 30){
            slog_warn(0, "warning: read fifo stalled during test");
        }

        if(test_cycle != NULL){
            (*test_cycle) = master_test_cycle;
        }

        if(master_test_failed){
            slog_error(0, "test failed at vector %llu out of %llu :(", master_test_cycle, total_unrolled_vecs);
        }else{
            if(master_test_cycle < total_unrolled_vecs || master_test_cycle > total_unrolled_vecs){
                slog_error(0, "test failed (executed %llu of %llu vectors) (fail flag didn't assert)!", master_test_cycle, total_unrolled_vecs);
                master_test_failed = true;
            }else{
                slog_info(0, "test PASS (executed %llu of %llu vectors)!", master_test_cycle, total_unrolled_vecs);
            }
        }

        if(master_test_failed){
            uint32_t num_fail_pins = 0;
            uint8_t *fail_pins = NULL;
            artix_get_stim_fail_pins(&fail_pins, &num_fail_pins);
            artix_print_stim_fail_pins(stim, fail_pins, num_fail_pins);
            free(fail_pins);
        }
    }

    struct gcore_registers *regs = NULL;
    regs = subcore_get_regs();
    print_regs(regs);

    return (master_test_failed || slave_test_failed);
}

/*
 * Configures the artix device with the bitstream bit file.
 *
 * Only takes a bitstream of type bin (flipped).
 *
 */
void artix_config(enum artix_selects artix_select, const char *bit_path){
    int fd;
    FILE *fp = NULL;
    off_t file_size;
    struct gcore_registers *regs;
    uint64_t *dma_buf;
    uint32_t mode_state;

    if(get_stim_type_by_path(bit_path) != STIM_TYPE_BIN){
        die("error: artix config only takes bin files (flipped): %s", bit_path);
    }

    if(util_fopen(bit_path, &fd, &fp, &file_size)){
        die("error: failed to open file '%s'", bit_path);
    }

    // alloc buffer and write bitstream to it.
    dma_buf = (uint64_t*)gcore_dma_alloc(file_size, sizeof(uint8_t));
    fread(dma_buf, sizeof(uint8_t), file_size, fp);

    // close since we don't need it anymore
    fclose(fp);
    close(fd);

    // load the state machine
    helper_subcore_load(artix_select, CONFIG_SETUP);

    //check for any init errors
    regs = subcore_get_regs();
    if(regs != NULL){
        if((regs->status & GCORE_STATUS_INIT_ERROR_MASK) == GCORE_STATUS_INIT_ERROR_MASK){
            die("error: failed to configure artix, init_error is high.");
        }
    }
    regs = subcore_free_regs(regs);

    // dma over the data from start of dma buffer sending file_size bytes
    
    if(artix_select == ARTIX_SELECT_A1){
        slog_info(0,"configuring a1 with %zu bytes...", (size_t)file_size);
    }else if (artix_select == ARTIX_SELECT_A2){
        slog_info(0,"configuring a2 with %zu bytes...", (size_t)file_size);
    }else{
        die("invalid artix select given");
    }
    fflush(stdout);
    gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, (size_t)file_size, NULL, 0);

    // doing subcore_state ioctl will write
    // config_num_bytes in the addr reg
    subcore_mode_state(&mode_state);
    regs = subcore_get_regs();

    //if(regs->addr != file_size){
    //    slog_error(0,"config: only %d bytes of %ld bytes sent.", regs->addr, file_size);
    //}

    subcore_idle();

    // check done
    if(regs != NULL){
        if((regs->status & GCORE_STATUS_DONE_ERROR_MASK) == GCORE_STATUS_DONE_ERROR_MASK){
            slog_error(0,"error: failed to configure, done error is high.");
        }else{
            if(artix_select == ARTIX_SELECT_A1){
                if((regs->a1_status & GCORE_AGENT_DONE_MASK) != GCORE_AGENT_DONE_MASK){
                    slog_error(0,"no done error, but a1 done pin did NOT go high.");
                }else{
                    slog_info(0,"done!");
                }
            }else if (artix_select == ARTIX_SELECT_A2){
                if((regs->a2_status & GCORE_AGENT_DONE_MASK) != GCORE_AGENT_DONE_MASK){
                    slog_error(0,"no done error, but a2 done pin did NOT go high.");
                }else{
                    slog_info(0,"done!");
                }
            }
        }
    }

    regs = subcore_free_regs(regs);

    return;
}
