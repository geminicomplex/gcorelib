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
	slog_debug(0, "subcore bursts: %d @ %d = %d bytes", BURST_BYTES, 
		num_bursts, (num_bursts*BURST_BYTES));
	helper_subcore_load_run(artix_select, SETUP_BURST);
	
	packet.rank_select = 0;
	packet.addr = 0;
	packet.data = num_bursts;
	gcore_ctrl_write(&packet);
	
	gcore_subcore_idle();

	slog_debug(0, "subcore: dma_write");
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
        die("error: address given is greater than (8589934592-1024)");
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

	// place memcore into write burst mode
    packet.rank_select = (uint32_t)((addr & 0x0000000100000000) >> 32);
    packet.addr = (uint32_t)(addr & 0x00000000ffffffff);
    packet.data = MEMCORE_BURST_CFG | MEMCORE_WRITE_BURST;
    helper_memcore_load_run(artix_select, &packet, num_bursts);
    
	// debug status
    helper_print_agent_status(artix_select);

	// load agent, gvpu and memcore with num bursts
	helper_num_bursts_load(artix_select, num_bursts);

	// debug status
    helper_print_agent_status(artix_select);

	// config gvpu to proxy data
    helper_gvpu_load_run(artix_select, MEM_WRITE);

	// debug status
    helper_print_agent_status(artix_select);

	// config agent to proxy data
    helper_agent_load_run(artix_select,  GVPU_WRITE);

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
        helper_get_agent_status(artix_select, &packet);
        uint32_t gvpu_burst_count = packet.addr + 1;
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
        helper_get_agent_status(artix_select, &packet);

        uint32_t gvpu_burst_count = packet.addr + 1;
        slog_info(0,"sent %zu bytes (actual %i).", 
            write_size, gvpu_burst_count*BURST_BYTES);

    }

//#ifdef GEM_DEBUG
//    for(int i=0; i<32;i++){
//		slog_debug(0,"dma_buf %02i: 0x%016"PRIX64"", i, write_data[i]);
//	}
//#endif

	// subcore must be idle
    gcore_subcore_idle();

    // reset burst count
    helper_gvpu_load_run(artix_select, TEST_CLEANUP);

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
            "the max size we can allocate %zu.", read_size, (size_t)MAX_CHUNK_SIZE);
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

    helper_gvpu_load_run(artix_select, MEM_READ);

    helper_agent_load_run(artix_select, GVPU_READ);

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
        helper_get_agent_status(artix_select, &packet);
        uint32_t gvpu_burst_count = packet.addr + 1;
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
		subcore_prep_dma_read(artix_select, size);
		gcore_dma_start(GCORE_WAIT_RX);
        memcpy(read_data, dma_buf, read_size);

        // get burst count from gvpu
        helper_get_agent_status(artix_select, &packet);
        uint32_t gvpu_burst_count = packet.addr + 1;
        slog_info(0,"received %zu bytes (actual %i).", 
            read_size, gvpu_burst_count*BURST_BYTES);
    }

//#ifdef GEM_DEBUG
//	for(int i=0; i<32;i++){
//		slog_debug(0,"dma_buf %02i: 0x%016"PRIX64"", i, read_data[i]);
//	}
//#endif

    // wait for idle state
    gcore_subcore_idle();

    // reset burst count
    helper_gvpu_load_run(artix_select, TEST_CLEANUP);

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
    //    die("error: calloc failed");
    //}

    // malloc read buffer
    if((read_data = (uint64_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint8_t))) == NULL){
        die("error: calloc failed");
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
            slog_debug(0, "printing write beats 5:7");
        } else {
            slog_debug(0, "printing write beats 0:2");
            start_beat = 16*0;
            end_beat = 16*2;
        }
	}else{
        start_beat = 0;
		end_beat = (MAX_CHUNK_SIZE/8);
	}
    for(int i=start_beat; i<end_beat;i++){
        slog_debug(0, "dma_buf %02i: 0x%016"PRIX64"", i, write_data[i]);
    }
#endif

    // always start the test at address zero
    uint32_t addr = 0x00000000;

    slog_debug(0, "writing mem data...");
    artix_mem_write(artix_select, addr, write_data, MAX_CHUNK_SIZE);

    // load agent, gvpu and memcore with num bursts
	helper_num_bursts_load(artix_select, num_bursts);

	// debug status
    helper_print_agent_status(artix_select);

    // performs crc on [0:5], check against 6th, and write to 7th
    // beat in each burst
    if(run_crc){
        slog_info(0, "loading crc test...");
        helper_gvpu_load_run(artix_select, MEM_TEST);
        helper_print_agent_status(artix_select);
    
        slog_info(0, "running crc test...");
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
        slog_info(0, "crc test finished.");
    }

	helper_print_agent_status(artix_select);

	slog_debug(0, "reset agent num bursts to 1...");
	helper_agent_load_run(artix_select, BURST_LOAD);
	helper_subcore_load_run(artix_select, CTRL_WRITE);
	packet.rank_select = 0;
	packet.addr = 0;
	packet.data = 1;
	gcore_ctrl_write(&packet);
	gcore_subcore_idle();

    // reset cycle count and failed flag
    helper_gvpu_load_run(artix_select, TEST_CLEANUP);

	helper_print_agent_status(artix_select);

    slog_info(0, "reading mem data...");
    artix_mem_read(artix_select, addr, read_data, MAX_CHUNK_SIZE);

    if(run_crc){
        if(crc_failed){
            slog_info(0, "mem test failed at word %d :(", ((crc_cycle*1024)/8));
        }else{
            slog_info(0, "mem test PASS (ran %i cycles)!", crc_cycle);
        }
    }

#ifdef GEM_DEBUG
	// print only max three beats from burst
	if((MAX_CHUNK_SIZE/8) > (16*2)){
        if(run_crc) {
            start_beat = 16*5;
            end_beat = 16*8;
            slog_debug(0, "printing read beats 5:7");
        } else {
            slog_debug(0, "printing read beats 0:2");
            start_beat = 16*0;
            end_beat = 16*2;
        }
	}else{
        start_beat = 0;
		end_beat = (MAX_CHUNK_SIZE/8);
	}
    for(int i=start_beat; i<end_beat;i++){
        slog_debug(0, "dma_buf %02i: 0x%016"PRIX64"", i, read_data[i]);
    }
#endif
        
    // re-generate write data with results not cleared out so 
    // it will match read_data
    write_data = util_get_static_data(MAX_CHUNK_SIZE, true, false);
    slog_info(0, "checking write/read array difference...");
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
        slog_info(0, "PASS!");
    }else{
        slog_info(0, "");
        slog_info(0, "found %llu differences starting at %i", (long long)found, starting);
        slog_info(0, "which is in chunk %i", ((starting*8)/DMA_SIZE));
    }

    slog_info(0, "done.");
    
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

static void prep_artix_for_test(struct stim *stim, enum artix_selects artix_select){
    struct gcore_ctrl_packet packet;
	uint64_t *dma_buf;
    uint32_t num_bursts;
    struct vec_chunk *chunk;
    struct profile_pin *pin = NULL;

    // check if dut_io_id is within range for given artix
    assert_dut_io_range(stim, artix_select);
    
    // perform test init
    helper_gvpu_load_run(artix_select, TEST_INIT);
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = (stim->num_vecs+stim->num_padding_vecs);
    helper_gvpu_packet_write(artix_select, &packet);

    // perform test setup by writing enable_pins burst
    helper_gvpu_load_run(artix_select, TEST_SETUP);
    helper_agent_load_run(artix_select, GVPU_WRITE);

	// reset dma buffer
    gcore_dma_alloc_reset();

    // send one burst of data (1024 bytes)
    num_bursts = 1;
	subcore_prep_dma_write(artix_select, num_bursts);
 
    size_t burst_size = num_bursts*BURST_BYTES;
    slog_info(0, "sending setup burst (%i bytes)...", burst_size);

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

    // write the enable pins to TEST_SETUP
    dma_buf = (uint64_t *)gcore_dma_alloc(burst_size, sizeof(uint8_t));
    memset(dma_buf, 0xffffffff, burst_size);
    memcpy(dma_buf, enable_pins, burst_size);

#ifdef GEM_DEBUG
    for(int i=0; i<32;i++){
        slog_debug(0, "pin_enable %02i: 0x%016"PRIX64"", i, dma_buf[i]);
    }
#endif

    gcore_dma_prep_start(GCORE_WAIT_TX, dma_buf, burst_size, NULL, 0);
    free(enable_pins);
    
    // reset cycle count and failed flag
    helper_gvpu_load_run(artix_select, TEST_CLEANUP);

    // address we are dma-ing to in the artix unit
    uint32_t addr = 0x00000000;

    slog_info(0, "writing vectors to memory...");
    // load one chunk at a time and dma the vecs to artix memory
    while((chunk = stim_load_next_chunk(stim, artix_select)) != NULL){

        // copy over the vec data buffer
        slog_info(0, "writing %i vecs (%zu bytes) to dma buffer...", chunk->num_vecs, chunk->vec_data_size);
        artix_mem_write(artix_select, addr, (uint64_t*)(chunk->vec_data), chunk->vec_data_size);

        // update the address pointer based on how much we copied in bytes
        addr += chunk->vec_data_size;
    }

    // reset test_cycle counter and test_failed flag
    helper_gvpu_load_run(artix_select, TEST_CLEANUP);

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
bool artix_dut_test(struct stim *stim, int64_t *test_cycle){
    struct gcore_ctrl_packet packet;
	bool test_failed = false;
    uint32_t ret_test_cycle = 0;
    enum artix_selects artix_select = ARTIX_SELECT_NONE;

    if(test_cycle != NULL){
        (*test_cycle) = -1;
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

    helper_print_agent_status(artix_select);
    
    // run the test
    slog_info(0, "running test...");
    helper_gvpu_load_run(artix_select, TEST_RUN);
    helper_print_agent_status(artix_select);

    uint32_t counter = 0;
    while(1){
        helper_get_agent_status(artix_select, &packet);
        if((packet.data & 0x000000f0) != (TEST_RUN << 4)){
            helper_print_agent_status(artix_select);
            break;
        }else{
            if(counter >= 0x000fffff){
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

    // msb byte is 0:did_stall:status_switch
    if((packet.addr & 0xf0000000) >> 30){
        slog_warn(0, "warning: read fifo stalled during test");
    }

    if(test_cycle != NULL){
        (*test_cycle) = ret_test_cycle;
    }

    if(test_failed){
        slog_error(0, "test failed at vector %d out of %i :(", 
                ret_test_cycle, (stim->num_unrolled_vecs+stim->num_padding_vecs));
    }else{
        if(ret_test_cycle < (stim->num_unrolled_vecs+stim->num_padding_vecs) || ret_test_cycle > (stim->num_unrolled_vecs+stim->num_padding_vecs)){
            slog_error(0, "test failed (executed %i of %i vectors)!", ret_test_cycle, (stim->num_unrolled_vecs+stim->num_padding_vecs));
            test_failed = true;
        }else{
            slog_info(0, "test PASS (executed %i of %i vectors)!", ret_test_cycle, (stim->num_unrolled_vecs+stim->num_padding_vecs));
        }
    }

    struct gcore_registers *regs = NULL;
    regs = gcore_get_regs();
    print_regs(regs);

    return test_failed;
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
    helper_subcore_load_run(artix_select, CONFIG_SETUP);

    //check for any init errors
    regs = gcore_get_regs();
    if(regs != NULL){
        if((regs->status & GCORE_STATUS_INIT_ERROR_MASK) == GCORE_STATUS_INIT_ERROR_MASK){
            die("error: failed to configure artix, init_error is high.");
        }
    }
    regs = gcore_free_regs(regs);

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

    // doing gcore_subcore_state ioctl will write
    // config_num_bytes in the addr reg
    gcore_subcore_mode_state(&mode_state);
    regs = gcore_get_regs();

    //if(regs->addr != file_size){
    //    slog_error(0,"config: only %d bytes of %ld bytes sent.", regs->addr, file_size);
    //}

    gcore_subcore_idle();

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

    regs = gcore_free_regs(regs);

    return;
}
