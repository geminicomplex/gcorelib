/*
 * Gemini board helper functions
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
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

#include "../common.h"
#include "../util.h"
#include "helper.h"
#include "subcore.h"
#include "driver.h"

/*
 * when subcore is in an idle state, load subcore with a given
 * state and run it.
 *
 */
void helper_subcore_load(enum artix_selects artix_select,
        enum subcore_states subcore_state){
    struct gcore_cfg gcfg;

    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: selecting both artix units not allowed");
    }

    // subcore must be in IDLE state to load and run
    subcore_idle();
    
    // load the state machine
    gcfg.subcore_state = subcore_state;
    gcfg.artix_select = artix_select;
    subcore_load(&gcfg);
    
    // subcore is paused, run!
    subcore_run();
    return;
}

/*
 * Let's the subcore gpio led module know that system boot successfully
 * finished.
 */
void helper_subcore_set_boot_done(){
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    struct gcore_ctrl_packet packet;

    helper_subcore_load(artix_select, GPIO_DNA);

    packet.rank_select = 0;
    packet.addr = SUBCORE_GPIO_DNA_CMD_BOOT_DONE;
    packet.data = 1; // doesn't do anything

    print_packet(&packet, "subcore_set_boot_done: ");

    // subcore will go to ctrl_write, agent will be idle
    subcore_write_packet(&packet);
    
    // wait for subcore idle state
    subcore_idle();

    return;
}

/*
 * Set's the subcore gpio led to given value.
 */
void helper_subcore_set_led(enum subcore_leds subcore_led, bool on){
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    struct gcore_ctrl_packet packet;

    if(subcore_led == SUBCORE_RED_LED){
        packet.addr = SUBCORE_GPIO_DNA_CMD_SET_RED_LED;
    } else if (subcore_led == SUBCORE_GREEN_LED){
        packet.addr = SUBCORE_GPIO_DNA_CMD_SET_GREEN_LED;
    } else {
        die("invalid led type given.");
    }

    packet.rank_select = 0;
    packet.data = (uint32_t)on;

    helper_subcore_load(artix_select, GPIO_DNA);

    print_packet(&packet, "subcore_set_led: ");
    
    // subcore will go to ctrl_write, agent will be idle
    subcore_write_packet(&packet);
    
    // wait for subcore idle state
    subcore_idle();

    return;
}

/*
 * Read the dna id from the dna_port..
 */
uint64_t helper_subcore_get_dna_id(){
    enum artix_selects artix_select = ARTIX_SELECT_NONE;
    struct gcore_ctrl_packet packet;
    uint64_t dna_id;

    // set packet to get dna
    packet.rank_select = 0;
    packet.addr = SUBCORE_GPIO_DNA_CMD_GET_DNA;
    packet.data = 1; // doesn't do anything

    helper_subcore_load(artix_select, GPIO_DNA);

    print_packet(&packet, "subcore_get_dna: ");
    
    // subcore will go to ctrl_write, agent will be idle
    subcore_write_packet(&packet);

    sleep(1);

    // clear the packet
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = 0;

    // read the packet with the dna value
    subcore_read_packet(&packet);

    // set upper and lower
    dna_id = 0;
    dna_id = dna_id | ((uint64_t)(packet.data) << 0);
    dna_id = dna_id | ((uint64_t)(packet.addr) << 32);
     
    // wait for subcore idle state
    subcore_idle();

    return dna_id;
}


/*
 * Peforms startup subroutine in agent, followed by 
 * loading and running the given fsm state, through
 * subcore to agent.
 *
 */
void helper_agent_load(enum artix_selects artix_select,
    enum agent_states agent_state){
    struct gcore_ctrl_packet packet;
    enum subcore_states subcore_state;
    struct gcore_registers *regs = NULL;
    bool agent_did_startup = false;

    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = 0;

    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: no artix unit given; selecting both not allowed");
    } else if(artix_select == ARTIX_SELECT_NONE) {
        die("error: no artix unit given");
    }

    // Need to check for error
    regs = subcore_get_regs();
    if(regs != NULL){
        if(artix_select == ARTIX_SELECT_A1){
            if((regs->a1_status & GCORE_AGENT_STARTUP_DONE_MASK) == GCORE_AGENT_STARTUP_DONE_MASK){
                agent_did_startup = true;
            }
        } else if (artix_select == ARTIX_SELECT_A2) {
            if((regs->a2_status & GCORE_AGENT_STARTUP_DONE_MASK) == GCORE_AGENT_STARTUP_DONE_MASK){
                agent_did_startup = true;
            }
        }
    }
    regs = subcore_free_regs(regs);

    /*
     * Only run state once. Each time it runs it asserts artix_reset_b and
     * performs initialization.
     *
     */
    if(!agent_did_startup) {
        if(artix_select == ARTIX_SELECT_A1){
            slog_info(0,"initializing agent a1...");
        }else if(artix_select == ARTIX_SELECT_A2){
            slog_info(0,"initializing agent a2...");
        }
        fflush(stdout);

        // wait for pll and mmcms to lock
        // init and calibrate ddr
        subcore_state = AGENT_STARTUP;
        helper_subcore_load(artix_select, subcore_state);

        sleep(2);

        // read agent status through ctrl_axi
        subcore_read_packet(&packet);

        print_packet(&packet, "agent startup: ");

        // Need to check for error
        regs = subcore_get_regs();
        if(regs != NULL){
            if((regs->status & GCORE_STATUS_INIT_ERROR_MASK) == GCORE_STATUS_INIT_ERROR_MASK){
                print_regs(regs);
                slog_error(0,"Agent startup init error.");
                exit(1);
            }
        }
        regs = subcore_free_regs(regs);
    }
    
    // fill packet
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = agent_state;

    // set subcore to proxy ctrl data
    subcore_state = CTRL_WRITE;
    helper_subcore_load(artix_select, subcore_state);
    
    // subcore will go to ctrl_write, agent will be idle
    subcore_write_packet(&packet);
    
    // wait for subcore idle state
    subcore_idle();
    
    // runs loaded agent state
    subcore_state = CTRL_RUN;
    helper_subcore_load(artix_select, subcore_state);
    
    // wait for subcore idle state
    subcore_idle();
    return;
}

/*
 * Place gvpu into MEM_BURST, place agent into BURST_LOAD
 * then pass num_bursts on data channel and it will config
 * agent, which will then auto-config gvpu/memcore.
 *
 */
void helper_burst_setup(enum artix_selects artix_select,
    uint64_t start_addr, uint32_t num_bursts){
    struct gcore_ctrl_packet packet;

    // load packet
    packet.rank_select = GET_START_RANK(start_addr);
    packet.addr = GET_START_ADDR(start_addr);
    packet.data = num_bursts;
    slog_info(0,"burst setup: %d @ %d = %d bytes @ addr 0x%X%08X", BURST_BYTES, 
        num_bursts, (num_bursts*BURST_BYTES), packet.rank_select, packet.addr);

    helper_memcore_load(artix_select, MEMCORE_SETUP_BURST);

    // load number of bursts into gvpu and memcore
    helper_gvpu_load(artix_select, MEM_BURST);
    
    // load  number of bursts into agent
    helper_agent_load(artix_select, BURST_LOAD);
    
    // set subcore to proxy ctrl data
    helper_subcore_load(artix_select, CTRL_WRITE);
    
    // load num_bursts into agent, which will auto-load num_bursts
    // into gvpu and memcore
    subcore_write_packet(&packet);

    // check if subcore is back to idle
    subcore_idle();
    return;

}

/*
 * Loads subcore, agent and gvpu to proxy the FSM state to 
 * load into memcore. Runs the state after loading it.
 *
 */
void helper_memcore_load(enum artix_selects artix_select, enum memcore_states memcore_state){
    enum gvpu_states gvpu_state;
    struct gcore_ctrl_packet packet;

    uint32_t data = 0x00000000;
    data |= memcore_state;

    gvpu_state = MEM_LOAD;
    helper_gvpu_load(artix_select, gvpu_state);
    
    helper_print_agent_status(artix_select);
    
    // check if gvpu is in mem_load state
    helper_get_agent_status(artix_select, &packet);
    if((packet.data & 0x000000f0) != 0x00000030){
        slog_error(0,"error: gvpu is not in MEM_LOAD state. 0x%08X", packet.data);
        exit(1);
    }

    // send the packet to MEM_LOAD
    uint32_t addr = 0x00000000;
    packet.rank_select = (uint32_t)((addr & 0x0000000100000000) >> 32);
    packet.addr = (uint32_t)(addr & 0x00000000ffffffff);
    packet.data = data;
    helper_gvpu_packet_write(artix_select, &packet);
    
    // run the loaded state
    gvpu_state = MEM_RUN;
    helper_gvpu_load(artix_select, gvpu_state);

    return;
}

/*
 * Call this after helper_memcore_load to check if memcore entered into the
 * desired state. When reading, memcore reads into a fifo buffer so if read
 * amount is less than fifo buffer size, memcore will exit the state and we
 * won't catch it here.
 *
 */
void helper_memcore_check_state(enum artix_selects artix_select, 
        enum memcore_states memcore_state, uint32_t num_bursts){
    struct gcore_ctrl_packet packet;
    // memcore is reading into a fifo so if the number of bursts 
    // read is less than the fifos capacity it will exit
    if((memcore_state == MEMCORE_READ_BURST && (num_bursts*BURST_BYTES) > ARTIX_READ_FIFO_BYTES)
        || memcore_state != MEMCORE_READ_BURST) {
        helper_get_agent_status(artix_select, &packet);
        if((packet.data & 0x00000f00) != (memcore_state << 8)){
            slog_error(0,"error: memcore is not in desired state. desired: 0x%08X actual: 0x%08X", 
                (memcore_state << 8), (packet.data & 0x00000f00));
            exit(1);
        }
    }

    return;
}

/* 
 * Loads gvpu with given fsm state through agent and
 * through subcore, then checks if subcore is idle.
 *
 */
void helper_gvpu_load(enum artix_selects artix_select,
        enum gvpu_states gvpu_state){
    struct gcore_ctrl_packet packet;
    enum agent_states agent_state;

    // fill packet
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = gvpu_state;
    print_gvpu_state(gvpu_state, "gvpu: ");

    helper_gvpu_packet_write(artix_select, &packet);
    
    agent_state = GVPU_RUN;
    helper_agent_load(artix_select, agent_state);
    
    // wait for subcore idle state
    subcore_idle();
    
    return;
}

/*
 * Performs a packet write through the ctrl axi-lite registers
 * from a gcore packet, through subcore and agent to gvpu.
 *
 */
void helper_gvpu_packet_write(enum artix_selects artix_select,
        struct gcore_ctrl_packet *packet){
    enum agent_states agent_state;
    enum subcore_states subcore_state;

    // set agent to proxy data
    agent_state = GVPU_LOAD;
    helper_agent_load(artix_select, agent_state);
    
    // set subcore to proxy ctrl data
    subcore_state = CTRL_WRITE;
    helper_subcore_load(artix_select, subcore_state);
    
    // write ctrl_axi
    subcore_write_packet(packet);
    
    // wait for subcore idle state
    subcore_idle();
    
    return;
}

uint64_t helper_get_agent_gvpu_status(enum artix_selects artix_select, 
        enum gvpu_status_selects select, enum gvpu_status_cmds cmd){
    struct gcore_ctrl_packet packet;
    uint64_t status = 0;

    
    helper_agent_load(artix_select, GVPU_STATUS);
    helper_subcore_load(artix_select, CTRL_WRITE);

    packet.rank_select = 0;
    packet.addr = (uint32_t) select;
    packet.data = (uint32_t) cmd;

    subcore_write_packet(&packet);

    sleep(1);

    // agent is in gvpu_cycle, do ctrl_read to grab data
    helper_subcore_load(artix_select, CTRL_READ);
    
    // read ctrl_axi
    subcore_read_packet(&packet);
    
    // wait for subcore idle state
    subcore_idle();

    status = 0;
    status = status | (((uint64_t)(packet.addr)) << 32);
    status = status | (((uint64_t)(packet.data)) << 0);

    return status;
}

/*
 * There are two ways to get the status of agent, one is through
 * a1/a2 status registers, and the second is to load agent with
 * the STATUS state and do a ctrl read, which is what this method
 * does.
 *
 */
void helper_get_agent_status(enum artix_selects artix_select, 
        struct gcore_ctrl_packet *packet){

    if(packet == NULL){
        die("pointer is NULL");
    }

    // clean packet
    packet->rank_select = 0;
    packet->addr = 0;
    packet->data = 0;

    enum agent_states agent_state = STATUS;
    helper_agent_load(artix_select, agent_state);
    
    // agent is in status, do ctrl_read to grab data
    enum subcore_states subcore_state = CTRL_READ;
    helper_subcore_load(artix_select, subcore_state);
    
    // read ctrl_axi
    subcore_read_packet(packet);
    
    // wait for subcore idle state
    subcore_idle();
    return;
}

/*
 * Get the agent status by loading the STATE into fsm and doing
 * ctrl read and then printing the received packet.
 *
 */
void helper_print_agent_status(enum artix_selects artix_select){
    struct gcore_ctrl_packet packet;
    helper_get_agent_status(artix_select, &packet);
    if(artix_select == ARTIX_SELECT_A1){
        print_packet(&packet, "a1 status: ");
    }else if(artix_select == ARTIX_SELECT_A2){
        print_packet(&packet, "a2 status: ");
    }else{
        die("no artix unit given.");
    }
    return;
}

/*
 * Prints the 7 gemini registers.
 *
 */
void print_regs(struct gcore_registers *regs){
    if(regs == NULL){
        return;
    }
    slog_info(0,"control: 0x%08X", regs->control);
    slog_info(0,"status: 0x%08X", regs->status);
    slog_info(0,"addr: 0x%08X", regs->addr);
    slog_info(0,"data: 0x%08X", regs->data);
    slog_info(0,"a1_status: 0x%08X", regs->a1_status);
    slog_info(0,"a2_status: 0x%08X", regs->a2_status);
    return;
}


/*
 * this is returned from zynq_core -> s_axi_ctrl
 *
 */
void _print_artix_status(uint32_t status){
    int fill             =  (status & 0x20000000)  >> 29;
    int status_valid     =  (status & 0x10000000)  >> 28;
    int mmcm_lock_error  = ~((status & 0x01000000) >> 24) & 0x1;
    int calib_error      = ~((status & 0x00800000) >> 23) & 0x1;
    int rst_error        =  (status & 0x00400000)  >> 22;
    int pll_lock_error   = ~((status & 0x00200000) >> 21) & 0x1;
    int temp_error       =  (status & 0x00100000)  >> 20;
    int startup_done     =  (status & 0x00080000)  >> 19;
    int done             =  (status & 0x00040000)  >> 18;
    int agent_error      =  (status & 0x00020000)  >> 17;
    int gvpu_fail        =  (status & 0x00010000)  >> 16;
    int stage            =  (status & 0x0000f000)  >> 12;
    int memcore          =  (status & 0x00000f00)  >> 8;
    int gvpu             =  (status & 0x000000f0)  >> 4;
    int agent            =  (status & 0x0000000f)  >> 0;
    printf("fill svalid mlock calib rst plock temp start done error fail stage memcore gvpu agent\n");
    printf("%-4d %-6d %-5d %-5d %-3d %-5d %-4d %-5d %-4d %-5d %-4d %-5d %-7d %-4d %-5d\n",
        fill, status_valid, mmcm_lock_error, calib_error, rst_error, pll_lock_error, temp_error,
        startup_done, done, agent_error, gvpu_fail, stage, memcore, gvpu, agent);
    return;
}

void print_regs_verbose(struct gcore_registers *regs){
    if(regs == NULL){
        die("pointer is null");
    }
    print_regs(regs);
    
    for(int i=0; i<80; i++){
        printf("-");
    }
    printf("\n");
    _print_artix_status(regs->a1_status);
    _print_artix_status(regs->a2_status);
    for(int i=0; i<80; i++){
        printf("-");
    }
    printf("\n");
    return;
}

/*
 * Prints a gcore ctrl packet to stdout.
 *
 */
void print_packet(struct gcore_ctrl_packet *packet, char *pre){
    if(packet == NULL){
        die("error: pointer is NULL");
    }
    if(pre == NULL){
        die("error: pointer is NULL");
    }
    slog_info(0,
        "%srank_sel: %X, addr: 0x%08X, data: 0x%08X",
        pre,
        packet->rank_select,
        packet->addr,
        packet->data
    );
    return;
}

/*
 * Writes the subcore mode and state to the given string.
 *
 */
void sprint_subcore_mode_state(char *mode_state_str) {
    uint32_t mode_state;

    if(mode_state_str == NULL){
        die("error: pointer is NULL");
    }

    subcore_mode_state(&mode_state);
    
    char mode_str[16];
    char state_str[256];

    switch(mode_state & 0xf0000000){
        case GCORE_ARTIX_SELECT_NONE:
            strcpy(mode_str, "none");
            break;
        case GCORE_ARTIX_SELECT_A1:
            strcpy(mode_str, "A1");
            break;
        case GCORE_ARTIX_SELECT_A2:
            strcpy(mode_str, "A2");
            break;
        default:
            // TODO: log warning here
            strcpy(mode_str, "invalid unit");
            break;
    };

    switch(mode_state & 0x0fffffff){
        case GCORE_SUBCORE_MODE_IDLE:
            strcpy(state_str, "idle"); break;
        case GCORE_SUBCORE_MODE_PAUSED:
            strcpy(state_str, "paused"); break;
        case GCORE_SUBCORE_MODE_CONFIG_SETUP:
            strcpy(state_str, "config_setup"); break;
        case GCORE_SUBCORE_MODE_CONFIG_LOAD:
            strcpy(state_str, "config_load"); break;
        case GCORE_SUBCORE_MODE_CONFIG_WAIT:
            strcpy(state_str, "config_wait"); break;
        case GCORE_SUBCORE_MODE_AGENT_STARTUP:
            strcpy(state_str, "agent_startup"); break;
        case GCORE_SUBCORE_MODE_SETUP_BURST:
            strcpy(state_str, "setup_burst"); break;
        case GCORE_SUBCORE_MODE_SETUP_WRITE:
            strcpy(state_str, "setup_write"); break;
        case GCORE_SUBCORE_MODE_SETUP_READ:
            strcpy(state_str, "setup_read"); break;
        case GCORE_SUBCORE_MODE_SETUP_CLEANUP:
            strcpy(state_str, "setup_cleanup"); break;
        case GCORE_SUBCORE_MODE_CTRL_WRITE:
            strcpy(state_str, "ctrl_write"); break;
        case GCORE_SUBCORE_MODE_CTRL_READ:
            strcpy(state_str, "ctrl_read"); break;
        case GCORE_SUBCORE_MODE_CTRL_RUN:
            strcpy(state_str, "ctrl_run"); break;
        case GCORE_SUBCORE_MODE_DMA_WRITE:
            strcpy(state_str, "dma_write"); break;
        case GCORE_SUBCORE_MODE_DMA_READ:
            strcpy(state_str, "dma_read"); break;
        case GCORE_SUBCORE_MODE_GPIO_DNA:
            strcpy(state_str, "gpio_dna"); break;

    };

    sprintf(mode_state_str, "subcore: mode=%s state=%s\n", mode_str, state_str);

    return;
}

enum agent_states get_agent_state(enum artix_selects artix_select, struct gcore_registers *regs){
    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: no artix unit given; selecting both not allowed");
    } else if(artix_select == ARTIX_SELECT_NONE) {
        die("error: no artix unit given");
    }

    if(regs == NULL){
        die("pointer is null");
    }

    enum agent_states agent_state = 0;
    if(artix_select == ARTIX_SELECT_A1){
        agent_state = (enum agent_states)((regs->a1_status & GCORE_AGENT_STATE_MASK) >> 0);
    }else if(artix_select == ARTIX_SELECT_A2){
        agent_state = (enum agent_states)((regs->a2_status & GCORE_AGENT_STATE_MASK) >> 0);
    }

    return agent_state;
}

/*
 * Given an agent state enum, print the state name to stdout. 
 *
 */
void print_agent_state(enum agent_states agent_state, char *pre){
    if(pre == NULL){
        return;
    }
    switch(agent_state){
        case AGENT_INIT:
            slog_info(0,"%sagent_init", pre);
            break;
        case AGENT_IDLE:
            slog_info(0,"%sagent_idle", pre);
            break;
        case AGENT_PAUSED:
            slog_info(0,"%sagent_paused", pre);
            break;
        case STATUS:
            slog_info(0,"%sstatus", pre);
            break;
        case BURST_LOAD:
            slog_info(0,"%sburst_load", pre);
            break;
        case GVPU_LOAD:
            slog_info(0,"%sgvpu_load", pre);
            break;
        case GVPU_RUN:
            slog_info(0,"%sgvpu_run", pre);
            break;
        case GVPU_WRITE:
            slog_info(0,"%sgvpu_write", pre);
            break;
        case GVPU_READ:
            slog_info(0,"%sgvpu_read", pre);
            break;
        case GVPU_STATUS:
            slog_info(0,"%sgvpu_status", pre);
            break;
        case GVPU_RESET:
            slog_info(0,"%sgvpu_reset", pre);
            break;
        default:
            break;
    };
    return;
}

enum gvpu_states get_gvpu_state(enum artix_selects artix_select, struct gcore_registers *regs){
    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: no artix unit given; selecting both not allowed");
    } else if(artix_select == ARTIX_SELECT_NONE) {
        die("error: no artix unit given");
    }

    if(regs == NULL){
        die("pointer is null");
    }

    enum gvpu_states gvpu_state = 0;
    if(artix_select == ARTIX_SELECT_A1){
        gvpu_state = (enum gvpu_states)((regs->a1_status & GCORE_AGENT_GVPU_STATE_MASK) >> 0);
    }else if(artix_select == ARTIX_SELECT_A2){
        gvpu_state = (enum gvpu_states)((regs->a2_status & GCORE_AGENT_GVPU_STATE_MASK) >> 0);
    }

    return gvpu_state;
}

/*
 * Given a gvpu state enum, print the state name to stdout.
 *
 */
void print_gvpu_state(enum gvpu_states gvpu_state, char *pre){
    if(pre == NULL){
        return;
    }
    switch(gvpu_state){
        case GVPU_IDLE:
            slog_info(0,"%sgvpu_idle", pre);
            break;
        case GVPU_PAUSED:
            slog_info(0,"%sgvpu_paused", pre);
            break;
        case MEM_BURST:
            slog_info(0,"%smem_burst", pre);
            break;
        case MEM_LOAD:
            slog_info(0,"%smem_load", pre);
            break;
        case MEM_RUN:
            slog_info(0,"%smem_run", pre);
            break;
        case MEM_WRITE:
            slog_info(0,"%smem_write", pre);
            break;
        case MEM_READ:
            slog_info(0,"%smem_read", pre);
            break;
        case MEM_TEST:
            slog_info(0,"%smem_test", pre);
            break;
        case TEST_INIT:
            slog_info(0,"%stest_init", pre);
            break;
        case TEST_SETUP:
            slog_info(0,"%stest_setup", pre);
            break;
        case TEST_RUN:
            slog_info(0,"%stest_run", pre);
            break;
        case TEST_FAIL_PINS:
            slog_info(0,"%stest_fail_pins", pre);
            break;
        case TEST_CLEANUP:
            slog_info(0,"%stest_cleanup", pre);
            break;
        default:
            break;
    }
    return;
}

enum memcore_states get_memcore_state(enum artix_selects artix_select, struct gcore_registers *regs){
    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: no artix unit given; selecting both not allowed");
    } else if(artix_select == ARTIX_SELECT_NONE) {
        die("error: no artix unit given");
    }

    if(regs == NULL){
        die("pointer is null");
    }

    enum memcore_states memcore_state = 0;
    if(artix_select == ARTIX_SELECT_A1){
        memcore_state = (enum memcore_states)((regs->a1_status & GCORE_AGENT_MEMCORE_STATE_MASK) >> 0);
    }else if(artix_select == ARTIX_SELECT_A2){
        memcore_state = (enum memcore_states)((regs->a2_status & GCORE_AGENT_MEMCORE_STATE_MASK) >> 0);
    }

    return memcore_state;
}

/*
 * Given a memcore state enum, print the state name to stdout.
 *
 */
void print_memcore_state(enum memcore_states memcore_state, char *pre){
    if(pre == NULL){
        return;
    }
    switch(memcore_state){
        case MEMCORE_IDLE:
            slog_info(0,"%smemcore_idle", pre);
            break;
        case MEMCORE_PAUSED:
            slog_info(0,"%smemcore_paused", pre);
            break;
        case MEMCORE_SETUP_BURST:
            slog_info(0,"%smemcore_setup_burst", pre);
            break;
        case MEMCORE_WRITE_BURST:
            slog_info(0,"%smemcore_write_burst", pre);
            break;
        case MEMCORE_READ_BURST:
            slog_info(0,"%smemcore_read_burst", pre);
            break;
        default:
            break;
    }
    return;
}


uint32_t get_gvpu_stage(enum artix_selects artix_select, struct gcore_registers *regs){
    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: no artix unit given; selecting both not allowed");
    } else if(artix_select == ARTIX_SELECT_NONE) {
        die("error: no artix unit given");
    }

    if(regs == NULL){
        die("pointer is null");
    }

    uint32_t stage = 0;
    if(artix_select == ARTIX_SELECT_A1){
        stage = ((regs->a1_status & GCORE_AGENT_GVPU_STAGE_MASK) >> 12);
    }else if(artix_select == ARTIX_SELECT_A2){
        stage = ((regs->a2_status & GCORE_AGENT_GVPU_STAGE_MASK) >> 12);
    }
    return stage;
}
