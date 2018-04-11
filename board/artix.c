/*
 * Methods to interact with the artix units.
 *   - configure units
 *   - write raw data to artix memory
 *   - read raw data to artix memory
 *   - run memory test
 *   - run dut test
 *
 */

#include "../common.h"
#include "../util.h"
#include "../stim.h"
#include "../profile.h"
#include "artix.h"
#include "helper.h"
#include "dma.h"
#include "subcore.h"

#include "../../driver/gcore_common.h"

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


static void subcore_prep_dma_write(enum artix_selects artix_select, uint32_t num_bursts){
	struct gcore_ctrl_packet packet;

	gcore_subcore_idle();

	// pass number of bursts to subcore
	printf("subcore bursts: %d @ %d = %d bytes\n", BURST_BYTES, 
		num_bursts, (num_bursts*BURST_BYTES));
	helper_subcore_load_run(artix_select, SETUP_BURST);
	
	packet.rank_select = 0;
	packet.addr = 0;
	packet.data = num_bursts;
	gcore_ctrl_write(&packet);
	
	gcore_subcore_idle();

	printf("subcore: dma_write\n");
    helper_subcore_load_run(artix_select, DMA_WRITE);

	return;
}

void artix_mem_write(enum artix_selects artix_select,
        uint64_t addr, uint64_t *write_data, size_t write_size){
    struct gcore_ctrl_packet packet;
	uint64_t *dma_buf;

    // address can't be greater than artix memory capacity
    // and you need to send at least one burst (1024 bytes)
    if(addr > (uint64_t)((8589934592)-BURST_BYTES)){
        die("error: address given is greater than (8589934592-1024)\n");
    }

    // can't allocate more than 250MB in one write chunk
    if(write_size > MAX_CHUNK_SIZE){
        die("error: write size %zu is greater than"
            "the max size we can allocate %zu.\n", write_size, (size_t)MAX_CHUNK_SIZE);
    }

    // each burst is 1024 bytes
    uint32_t num_bursts = (write_size / BURST_BYTES);

    // extra data so add one more burst
    if((write_size % BURST_BYTES) != 0){
        num_bursts = num_bursts + 1;
    }

	// place memcore into write burst mode
    packet.rank_select = (uint32_t)((addr & 0x0000000100000000) >> 32);
    packet.addr = (uint32_t)(addr & 0x00000000ffffffff);
    packet.data = MEMCORE_BURST_CFG | MEMCORE_WRITE_BURST;
    helper_memcore_load_run(artix_select, &packet, num_bursts);
    
	// debug status
    helper_print_agent_status(artix_select);

	// load agent, dutcore and memcore with num bursts
	helper_num_bursts_load(artix_select, num_bursts);

	// debug status
    helper_print_agent_status(artix_select);

	// config dutcore to proxy data
    helper_dutcore_load_run(artix_select, MEM_WRITE);

	// debug status
    helper_print_agent_status(artix_select);

	// config agent to proxy data
    helper_agent_load_run(artix_select,  DUT_WRITE);

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
            printf("writing %i bytes...\n", DMA_SIZE);
            memset(dma_buf, 0, DMA_SIZE);
            memcpy(dma_buf, ((uint8_t*)write_data)+(i*DMA_SIZE), DMA_SIZE);
            gcore_dma_prep(dma_buf, DMA_SIZE, NULL, 0);
			gcore_dma_start(GCORE_WAIT_TX);
        }

        // send rest of data
        size_t data_mod = write_size % DMA_SIZE;
        if(data_mod != 0){
            printf("writing %zu bytes...\n", data_mod);
            memcpy(dma_buf, ((uint8_t*)write_data)+(write_size-data_mod), data_mod);
            gcore_dma_prep(dma_buf, data_mod, NULL, 0);
			gcore_dma_start(GCORE_WAIT_TX);
        }

        // get burst count from dutcore
        helper_get_agent_status(artix_select, &packet);
        uint32_t dutcore_burst_count = packet.addr + 1;
        printf("sent %zu total bytes (actual %i).\n", 
            (num_chunks*DMA_SIZE)+data_mod, 
            dutcore_burst_count*BURST_BYTES);

    }else{
        // this always ensures that what we read is a multiple of a burst
        // since num_bursts is calculated based on write_size
        size_t size = num_bursts*BURST_BYTES; 
        printf("writing %zu bytes (actual %zu)...\n", write_size, size);
        dma_buf = (uint64_t *)gcore_dma_alloc(size, sizeof(uint8_t));
        memset(dma_buf, 0, size);
        memcpy(dma_buf, write_data, write_size);
        gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, size, NULL, 0);
        
        // get burst count from dutcore
        helper_get_agent_status(artix_select, &packet);

        uint32_t dutcore_burst_count = packet.addr + 1;
        printf("sent %zu bytes (actual %i).\n", 
            write_size, dutcore_burst_count*BURST_BYTES);

    }

//#ifdef GEM_DEBUG
//    for(int i=0; i<32;i++){
//		printf("dma_buf %02i: 0x%016"PRIX64"\n", i, write_data[i]);
//	}
//#endif

	// subcore must be idle
    gcore_subcore_idle();

    // reset burst count
    helper_dutcore_load_run(artix_select, TEST_CLEANUP);

	// debug status
    helper_print_agent_status(artix_select);
    return;
}

// subcore must assert last on the last burst of the last beat and a dma
// descriptor transfer can't be more than 2^23-1 due to axi dma ip
// limitation. If transfering more, than 8MB just keep calling this. For
// agent, dutcore and memcore you can just set max num_bursts.
// TODO: modify subcore to automatically send last
// if it's num_bursts > 65504
static void subcore_prep_dma_read(enum artix_selects artix_select, uint32_t num_bursts){
	struct gcore_ctrl_packet packet;

	// (2^23 - (4096 linux page) / 128) = 65504
	if(num_bursts > 65504){
		die("gcore: subcore_prep_dma_read error, can't set num bursts: %i > 65535\n", num_bursts);
	}

	gcore_subcore_idle();

	// kernel sends bursts of 128 bytes to subcore. However
	// artix core operates with bursts of 1024 bytes so the
	// number of bursts will be different.
    helper_subcore_load_run(artix_select, SETUP_BURST);

    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = num_bursts;
    gcore_ctrl_write(&packet);

    gcore_subcore_idle();

    helper_subcore_load_run(artix_select, DMA_READ);
	return;
}

void artix_mem_read(enum artix_selects artix_select, uint64_t addr,
        uint64_t *read_data, size_t read_size){
	uint64_t *dma_buf;
    struct gcore_ctrl_packet packet;

    // 
    if(read_size > MAX_CHUNK_SIZE){
        die("error: read size %zu is greater than"
            "the max size we can allocate %zu.\n", read_size, (size_t)MAX_CHUNK_SIZE);
    }

	// kernel sends bursts of 128 bytes to subcore.
    uint32_t num_bursts = read_size / BURST_BYTES;

    // extra data so add one more burst
	if((read_size % BURST_BYTES) != 0){
		num_bursts = num_bursts + 1;
	}

    // load num bursts before memcore is placed into read mode
    helper_num_bursts_load(artix_select, num_bursts);

	helper_print_agent_status(artix_select);

	// fill packet
    packet.rank_select = (uint32_t)((addr & 0x0000000100000000) >> 32);
    packet.addr = (uint32_t)(addr & 0x00000000ffffffff);
    packet.data = MEMCORE_BURST_CFG | MEMCORE_READ_BURST;
	helper_memcore_load_run(artix_select, &packet, num_bursts);

	helper_print_agent_status(artix_select);

    helper_dutcore_load_run(artix_select, MEM_READ);

    helper_agent_load_run(artix_select, DUT_READ);

    gcore_dma_alloc_reset();

    if(read_size > DMA_SIZE){
        // allocate largest possible dma buffer
        dma_buf = (uint64_t *)gcore_dma_alloc(DMA_SIZE, sizeof(uint8_t));
        memset(dma_buf, 0, DMA_SIZE);

		uint32_t num_chunks = (read_size/DMA_SIZE);

		// receive chunks of DMA_SIZE
        for(int i=0; i<num_chunks;i++){
            printf("reading %i bytes...\n", DMA_SIZE);
            memset(dma_buf, 0, DMA_SIZE);
            gcore_dma_prep(NULL, 0, dma_buf, DMA_SIZE);
			subcore_prep_dma_read(artix_select, DMA_SIZE/BURST_BYTES);
			gcore_dma_start(GCORE_WAIT_RX);
            memcpy(((uint8_t*)read_data)+(i*DMA_SIZE), dma_buf, DMA_SIZE);
        }

        // send the rest
        size_t data_mod = read_size % DMA_SIZE;
        if(data_mod != 0){
            printf("reading %zu bytes...\n", data_mod);
            gcore_dma_prep(NULL, 0, dma_buf, data_mod);
			subcore_prep_dma_read(artix_select, data_mod/BURST_BYTES);
			gcore_dma_start(GCORE_WAIT_RX);
            memcpy(((uint8_t*)read_data)+(read_size-data_mod), dma_buf, data_mod);
        }

        // get burst count from dutcore
        helper_get_agent_status(artix_select, &packet);
        uint32_t dutcore_burst_count = packet.addr + 1;
        printf("received %zu total bytes (actual %i).\n", 
            (num_chunks*DMA_SIZE)+data_mod, 
            dutcore_burst_count*BURST_BYTES);
    }else{
        // this always ensures that what we read is a multiple of a burst
        // since num_bursts is calculated based on read_size
        size_t size = num_bursts*BURST_BYTES; 
        printf("reading %zu bytes (actual %zu)...\n", read_size, size);
        dma_buf = (uint64_t *)gcore_dma_alloc(size, sizeof(uint8_t));
        memset(dma_buf, 0, size);
        gcore_dma_prep(NULL, 0, dma_buf, size);
		subcore_prep_dma_read(artix_select, size);
		gcore_dma_start(GCORE_WAIT_RX);
        memcpy(read_data, dma_buf, read_size);

        // get burst count from dutcore
        helper_get_agent_status(artix_select, &packet);
        uint32_t dutcore_burst_count = packet.addr + 1;
        printf("received %zu bytes (actual %i).\n", 
            read_size, dutcore_burst_count*BURST_BYTES);
    }

//#ifdef GEM_DEBUG
//	for(int i=0; i<32;i++){
//		printf("dma_buf %02i: 0x%016"PRIX64"\n", i, read_data[i]);
//	}
//#endif

    // wait for idle state
    gcore_subcore_idle();

    // reset burst count
    helper_dutcore_load_run(artix_select, TEST_CLEANUP);

    helper_print_agent_status(artix_select);
    return;
}

void artix_mem_test(enum artix_selects artix_select, bool run_crc){
#ifdef GEM_DEBUG
    uint32_t start_beat = 0;
    uint32_t end_beat = 0;
#endif
    uint64_t *write_data = NULL;
    uint64_t *read_data = NULL;
	struct gcore_ctrl_packet packet;
	bool crc_failed = false;
	uint32_t crc_cycle = 0;

    // each burst is 1024 bytes
    uint32_t num_bursts = (MAX_CHUNK_SIZE / BURST_BYTES);

    // extra data so add one more burst
    if((MAX_CHUNK_SIZE % BURST_BYTES) != 0){
        num_bursts = num_bursts + 1;
    }

    // malloc write buffer
    //if((write_data = (uint64_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint8_t))) == NULL){
    //    die("error: calloc failed\n");
    //}

    // malloc read buffer
    if((read_data = (uint64_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint8_t))) == NULL){
        die("error: calloc failed\n");
    }
    
    if(run_crc){
        // include xor data and clear last beat in burst
	    write_data = util_get_static_data(MAX_CHUNK_SIZE, true, true);
    }else if(!run_crc){
        // include xor data and write to last beat as well
	    write_data = util_get_static_data(MAX_CHUNK_SIZE, true, false);
    }

#ifdef GEM_DEBUG
	// print only max three beats from burst
	if((MAX_CHUNK_SIZE/8) > (16*2)){
        if(run_crc) {
            start_beat = 16*5;
            end_beat = 16*8;
            printf("printing write beats 5:7\n");
        } else {
            printf("printing write beats 0:2\n");
            start_beat = 16*0;
            end_beat = 16*2;
        }
	}else{
        start_beat = 0;
		end_beat = (MAX_CHUNK_SIZE/8);
	}
    for(int i=start_beat; i<end_beat;i++){
        printf("dma_buf %02i: 0x%016"PRIX64"\n", i, write_data[i]);
    }
#endif

    // always start the test at address zero
    uint32_t addr = 0x00000000;

    printf("writing mem data...\n");
    artix_mem_write(artix_select, addr, write_data, MAX_CHUNK_SIZE);

    // load agent, dutcore and memcore with num bursts
	helper_num_bursts_load(artix_select, num_bursts);

	// debug status
    helper_print_agent_status(artix_select);

    // performs crc on [0:5], check against 6th, and write to 7th
    // beat in each burst
    if(run_crc){
        printf("loading crc test...\n");
        helper_dutcore_load_run(artix_select, MEM_TEST);
        helper_print_agent_status(artix_select);
    
        printf("running crc test...\n");
        uint32_t counter = 0;
        while(1){
            helper_get_agent_status(artix_select, &packet);
            if((packet.data & 0x000000f0) != (MEM_TEST << 4)){
                helper_print_agent_status(artix_select);
                break;
            }else{
                if(counter >= 0x000000ff){
                    helper_get_agent_status(artix_select, &packet);
                    crc_cycle = packet.addr;
                    helper_print_agent_status(artix_select);
                    counter = 0;
                    break;
                }else{
                    counter = counter + 1;
                }
            }
        }

        // grab number of test cycles and if it failed
        helper_get_agent_status(artix_select, &packet);
        crc_cycle = packet.addr;
        if((packet.data & 0x000f0000) == 0x00010000){
            crc_failed = true;
        }
        printf("crc test finished.\n");
    }

	helper_print_agent_status(artix_select);

	printf("reset agent num bursts to 1...\n");
	helper_agent_load_run(artix_select, BURST_LOAD);
	helper_subcore_load_run(artix_select, CTRL_WRITE);
	packet.rank_select = 0;
	packet.addr = 0;
	packet.data = 1;
	gcore_ctrl_write(&packet);
	gcore_subcore_idle();

    // reset cycle count and failed flag
    helper_dutcore_load_run(artix_select, TEST_CLEANUP);

	helper_print_agent_status(artix_select);

    printf("reading mem data...\n");
    artix_mem_read(artix_select, addr, read_data, MAX_CHUNK_SIZE);

    if(run_crc){
        if(crc_failed){
            printf("mem test failed at word %d :(\n", ((crc_cycle*1024)/8));
        }else{
            printf("mem test PASS (ran %i cycles)!\n", crc_cycle);
        }
    }

#ifdef GEM_DEBUG
	// print only max three beats from burst
	if((MAX_CHUNK_SIZE/8) > (16*2)){
        if(run_crc) {
            start_beat = 16*5;
            end_beat = 16*8;
            printf("printing read beats 5:7\n");
        } else {
            printf("printing read beats 0:2\n");
            start_beat = 16*0;
            end_beat = 16*2;
        }
	}else{
        start_beat = 0;
		end_beat = (MAX_CHUNK_SIZE/8);
	}
    for(int i=start_beat; i<end_beat;i++){
        printf("dma_buf %02i: 0x%016"PRIX64"\n", i, read_data[i]);
    }
#endif
        
    // re-generate write data with results not cleared out so 
    // it will match read_data
    write_data = util_get_static_data(MAX_CHUNK_SIZE, true, false);
    printf("checking write/read array difference...");
    uint32_t starting = 0;
    uint32_t found = 0;
    for(int i=0; i<(MAX_CHUNK_SIZE/sizeof(uint64_t)); i++){
        if(write_data[i] != read_data[i]){
            if(!starting){
                starting = i;
            }
            found = found + 1;
        }
    }
    if(found == 0){
        printf("PASS!\n");
    }else{
        printf("\n");
        printf("found %llu differences starting at %i\n", (long long)found, starting);
        printf("which is in chunk %i\n", ((starting*8)/DMA_SIZE));
    }

    printf("done.\n");
    
    return;
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
        die("error: pointer is NULL\n");
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
            die("error: dut_io_id %i is out of range for artix unit\n", pin->dut_io_id);
        }
    }
    return;
}

static void prep_artix_for_test(struct stim *stim, enum artix_selects artix_select){
    struct gcore_ctrl_packet packet;
	uint64_t *dma_buf;
    uint32_t num_bursts;
    uint32_t dutcore_burst_count;
    struct vec_chunk *chunk;
    struct profile_pin *pin = NULL;

    // check if dut_io_id is within range for given artix
    assert_dut_io_range(stim, artix_select);
    
    // perform test init
    helper_dutcore_load_run(artix_select, TEST_INIT);
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = stim->num_vecs;
    helper_dutcore_packet_write(artix_select, &packet);

    // perform test setup by writing enable_pins burst
    helper_dutcore_load_run(artix_select, TEST_SETUP);
    helper_agent_load_run(artix_select, DUT_WRITE);

	// reset dma buffer
    gcore_dma_alloc_reset();

    // send one burst of data (1024 bytes)
    num_bursts = 1;
	subcore_prep_dma_write(artix_select, num_bursts);
 
    printf("sending setup burst...\n");
    size_t burst_size = num_bursts*BURST_BYTES;

    // clear all pins by FFing them. This will cause test_run to not 
    // process those pins.
    uint8_t *enable_pins = NULL;
    uint32_t num_enable_pins = 256;
    if((enable_pins = (uint8_t*)calloc(burst_size, sizeof(uint8_t))) == NULL){
        die("error: calloc failed\n");
    }
    for(int i=0; i<num_enable_pins; i++){
        enable_pins[i] = 0xff;
    }

    // turn on only the pins we're using based on the found dut_io_ids
    for(int pin_id=0; pin_id<stim->num_pins; pin_id++){
        pin = stim->pins[pin_id];

        // clamp the id from 0 to 200 since we're only writing to one
        // dut at a time and so packed_subvecs will always be len of 200
        uint8_t dut_io_id = (uint8_t)(pin->dut_io_id % DUT_NUM_PINS);

        enable_pins[dut_io_id] = 0x00;

#ifdef GEM_DEBUG
        printf("%s : %i\n", pin->net_alias, dut_io_id);
#endif
    }

    // Note: no need to swap the endianess of enable_pins because of the
    // way 64 bit words are packed in agent, dutcore and memcore

    // write the enable pins to TEST_SETUP
    dma_buf = (uint64_t *)gcore_dma_alloc(burst_size, sizeof(uint8_t));
    memset(dma_buf, 0xffffffff, burst_size);
    memcpy(dma_buf, enable_pins, burst_size);

#ifdef GEM_DEBUG
    for(int i=0; i<32;i++){
        printf("pin_enable %02i: 0x%016"PRIX64"\n", i, dma_buf[i]);
    }
#endif

    gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, burst_size, NULL, 0);
    free(enable_pins);
    
    // get burst count from dutcore
    helper_get_agent_status(artix_select, &packet);
    dutcore_burst_count = packet.addr + 1;
    printf("sent setup burst (%i bytes).\n", dutcore_burst_count*BURST_BYTES);

    // reset cycle count and failed flag
    helper_dutcore_load_run(artix_select, TEST_CLEANUP);

    // address we are dma-ing to in the artix unit
    uint32_t addr = 0x00000000;

    printf("writing vectors to memory...\n");
    // load one chunk at a time and dma the vecs to artix memory
    while((chunk = stim_load_next_chunk(stim, artix_select)) != NULL){

        // copy over the vec data buffer
        printf("writing %i vecs (%zu bytes) to dma buffer...\n", chunk->num_vecs, chunk->vec_data_size);
        artix_mem_write(artix_select, addr, (uint64_t*)(chunk->vec_data), chunk->vec_data_size);

        // update the address pointer based on how much we copied in bytes
        addr += chunk->vec_data_size;
    }

    // reset test_cycle counter and test_failed flag
    helper_dutcore_load_run(artix_select, TEST_CLEANUP);

}

//
// Given a dots, rbt, bin, bit or stim path, create a stim file and
// prep for vec chunks to be loaded. For each loaded chunk, fill the
// chunk, write the raw packed vectors to artix memory, until all chunks
// written. Perform the test. Read back the results into the vec chunks.
// Write the stim lite to disk.
// 
//
// For now we just write raw vectors to memory and execute directly,
// which gives us a max of around 67 million vectors. Instead, we should
// have a simple virtual machine running so we can do jumps in memory to
// perform loops in hardware.
//
// returns -1 if pass or failing cycle number (zero indexed)
//
//
int64_t artix_dut_test(struct stim *stim){
    struct gcore_ctrl_packet packet;
	bool test_failed = false;
	int64_t test_cycle = -1;
    uint32_t ret_test_cycle = 0;
    enum artix_selects artix_select = ARTIX_SELECT_NONE;

    if(stim == NULL){
        die("pointer is NULL\n");
    }

    if(stim->profile == NULL){
        die("no profile set for stim\n");
    }

    if(!stim->num_a1_vec_chunks && !stim->num_a2_vec_chunks){
        die("stim has neither a1 nor a2 chunks");
    }

    bool dual_mode = false;
    if(stim->num_a1_vec_chunks > 0 && stim->num_a2_vec_chunks > 0){
        dual_mode = true;
        prep_artix_for_test(stim, ARTIX_SELECT_A1);
        prep_artix_for_test(stim, ARTIX_SELECT_A2);

        // a1 is always the master in dual_mode
        artix_select = ARTIX_SELECT_A1;

        die("dual mode stims are not currently supported");
    }else if(stim->num_a1_vec_chunks > 0){
        prep_artix_for_test(stim, ARTIX_SELECT_A1);
        artix_select = ARTIX_SELECT_A1;
    }else if(stim->num_a2_vec_chunks > 0){
        prep_artix_for_test(stim, ARTIX_SELECT_A2);
        artix_select = ARTIX_SELECT_A2;
    }

    if(dual_mode){
        // setup a1 and a2 for dual mode
    }
    
    // run the test
    printf("running test...\n");
    helper_dutcore_load_run(artix_select, TEST_RUN);
    helper_print_agent_status(artix_select);

    uint32_t counter = 0;
    while(1){
        helper_get_agent_status(artix_select, &packet);
        if((packet.data & 0x000000f0) != (TEST_RUN << 4)){
            helper_print_agent_status(artix_select);
            break;
        }else{
            if(counter >= 0x000000ff){
                helper_get_agent_status(artix_select, &packet);
                ret_test_cycle = packet.addr;
                helper_print_agent_status(artix_select);
                counter = 0;
                break;
            }else{
                counter = counter + 1;
            }
        }
    }

    // grab number of test cycles and if it failed
    helper_get_agent_status(artix_select, &packet);
    ret_test_cycle = (packet.addr & 0x0fffffff);
    if((packet.data & 0x000f0000) == 0x00010000){
        test_failed = true;
    }
    if(packet.addr & 0xf0000000){
        printf("warning: read fifo stalled during test\n");
    }
    if(test_failed){
        printf("test failed at vector %d out of %i :(\n", ret_test_cycle, stim->num_unrolled_vecs);
        test_cycle = ret_test_cycle;
    }else{
        printf("test PASS (executed %i of %i vectors)!\n", ret_test_cycle, stim->num_unrolled_vecs);
    }

    printf("done!\n");

    return test_cycle;
}

/*
 * Configures the artix device with the bitstream bit file.
 *
 */
void artix_config(enum artix_selects artix_select, const char *bit_path){
    int fd;
    FILE *fp = NULL;
    off_t file_size;
    struct gcore_registers *regs;
	uint64_t *dma_buf;
    uint32_t mode_state;

    if(util_fopen(bit_path, &fd, &fp, &file_size)){
        die("error: failed to open file '%s'\n", bit_path);
    }

    // alloc buffer and write bitstream to it.
    dma_buf = (uint64_t*)gcore_dma_alloc(file_size, sizeof(uint8_t));
    fread(dma_buf, sizeof(uint8_t), file_size, fp);

    // close since we don't need it anymore
    fclose(fp);
    close(fd);

    // load the state machine
    helper_subcore_load_run(artix_select, CONFIG_SETUP);

    //check for any init errors
    regs = gcore_get_regs();
    if(regs != NULL){
        if((regs->status & GCORE_STATUS_INIT_ERROR_MASK) == GCORE_STATUS_INIT_ERROR_MASK){
            die("error: failed to configure artix, init_error is high.\n");
        }
    }
    regs = gcore_free_regs(regs);

    // dma over the data from start of dma buffer sending file_size bytes
    printf("configuring with %zu bytes...", (size_t)file_size);
    fflush(stdout);
    gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, (size_t)file_size, NULL, 0);
    printf("done.\n");

    // doing gcore_subcore_state ioctl will write
    // config_num_bytes in the addr reg
    gcore_subcore_mode_state(&mode_state);
    regs = gcore_get_regs();

    //if(regs->addr != file_size){
    //    printf("config: error, only %d bytes of %ld bytes sent.\n", regs->addr, file_size);
    //}

    gcore_subcore_idle();

    // check done
    if(regs != NULL){
        if((regs->status & GCORE_STATUS_DONE_ERROR_MASK) == GCORE_STATUS_DONE_ERROR_MASK){
            printf("error: failed to configure, done error is high.\n");
        }else{
            if(artix_select == ARTIX_SELECT_A1){
                if((regs->a1_status & GCORE_AGENT_DONE_MASK) != GCORE_AGENT_DONE_MASK){
                    printf("no done error, but a1 done pin did NOT go high.\n");
                }else{
                    printf("a1 done went high!\n");
                }
            }else if (artix_select == ARTIX_SELECT_A2){
                if((regs->a2_status & GCORE_AGENT_DONE_MASK) != GCORE_AGENT_DONE_MASK){
                    printf("no done error, but a2 done pin did NOT go high.\n");
                }else{
                    printf("a2 done went high!\n");
                }
            }
        }
    }

    regs = gcore_free_regs(regs);

    return;
}
