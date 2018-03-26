/*
 * Gemini helper
 *
 */

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "../common.h"
#include "../util.h"
#include "helper.h"
#include "subcore.h"

#include "../../driver/gcore_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>


/*
 * when subcore is in an idle state, load subcore with a given
 * state and run it.
 *
 */
void helper_subcore_load_run(enum artix_selects artix_select,
        enum subcore_states subcore_state){
    struct gcore_cfg gcfg;

    if (artix_select == ARTIX_SELECT_BOTH) {
        die("error: selecting both artix units not allowed\n");
    }

    // subcore must be in IDLE state to load and run
    gcore_subcore_idle();
    
    // load the state machine
    gcfg.subcore_state = subcore_state;
    gcfg.artix_select = artix_select;
    gcore_subcore_load(&gcfg);
    
    // subcore is paused, run!
    gcore_subcore_run();
    return;
}

/*
 * Peforms startup subroutine in agent, followed by 
 * loading and running the given fsm state, through
 * subcore to agent.
 *
 */
void helper_agent_load_run(enum artix_selects artix_select,
    enum agent_states agent_state){
    struct gcore_ctrl_packet packet;
    enum subcore_states subcore_state;
    struct gcore_registers *regs = NULL;
    bool agent_did_startup = false;

    // Need to check for error
    regs = gcore_get_regs();
    if(regs != NULL){

        if(artix_select == ARTIX_SELECT_A1){
            if((regs->a1_status & GCORE_AGENT_STARTUP_DONE_MASK) == GCORE_AGENT_STARTUP_DONE_MASK){
                agent_did_startup = true;
            }
        } else if (artix_select == ARTIX_SELECT_A2) {
            if((regs->a2_status & GCORE_AGENT_STARTUP_DONE_MASK) == GCORE_AGENT_STARTUP_DONE_MASK){
                agent_did_startup = true;
            }
        } else if (artix_select == ARTIX_SELECT_BOTH) {
            die("error: no artix unit given; selecting both not allowed\n");
        } else {
            die("error: no artix unit given\n");
        }
    }
    regs = gcore_free_regs(regs);

    /*
     * Only run state once. Each time it runs it asserts artix_reset_b and
     * performs initialization.
     *
     */
    if(!agent_did_startup) {
        if(artix_select == ARTIX_SELECT_A1){
            printf("initializing agent a1...");
        }else if(artix_select == ARTIX_SELECT_A2){
            printf("initializing agent a2...");
        }
        fflush(stdout);
        // wait for pll and mmcms to lock
        // init and calibrate ddr
        subcore_state = AGENT_STARTUP;
        helper_subcore_load_run(artix_select, subcore_state);

        sleep(2);

        // read agent status through ctrl_axi
        gcore_ctrl_read(&packet);

        printf("done.\n");
        print_packet(&packet, "start: ");

        // Need to check for error
        regs = gcore_get_regs();
        if(regs != NULL){
            if((regs->status & GCORE_STATUS_INIT_ERROR_MASK) == GCORE_STATUS_INIT_ERROR_MASK){
                print_regs(regs);
                printf("Agent startup init error.\n");
                exit(1);
            }
        }
        regs = gcore_free_regs(regs);
    }
    
    // fill packet
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = agent_state;

    // set subcore to proxy ctrl data
    subcore_state = CTRL_WRITE;
    helper_subcore_load_run(artix_select, subcore_state);
    
    // subcore will go to ctrl_write, agent will be idle
    gcore_ctrl_write(&packet);
    
    // wait for subcore idle state
    gcore_subcore_idle();
    
    // runs loaded agent state
    subcore_state = CTRL_RUN;
    helper_subcore_load_run(artix_select, subcore_state);
    
    // wait for subcore idle state
    gcore_subcore_idle();
    return;
}

/*
 * Place dutcore into MEM_BURST, place agent into BURST_LOAD
 * then pass num_bursts on data channel and it will config
 * agent, which will then auto-config dutcore/memcore.
 *
 */
void helper_num_bursts_load(enum artix_selects artix_select,
    uint32_t num_bursts){
    enum dutcore_states dutcore_state;
    enum agent_states agent_state;
    enum subcore_states subcore_state;
    struct gcore_ctrl_packet packet;

    // load packet
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = num_bursts;
    printf("agent burst: %d @ %d = %d bytes\n", BURST_BYTES, 
        num_bursts, (num_bursts*BURST_BYTES));

    // load number of bursts into dutcore and memcore
    dutcore_state = MEM_BURST;
    helper_dutcore_load_run(artix_select, dutcore_state);
    
	// load  number of bursts into agent
    agent_state = BURST_LOAD;
    helper_agent_load_run(artix_select, agent_state);
    
    // set subcore to proxy ctrl data
    subcore_state = CTRL_WRITE;
    helper_subcore_load_run(artix_select, subcore_state);
    
    // load num_bursts into agent, which will auto-load num_bursts
    // into dutcore and memcore
    gcore_ctrl_write(&packet);
    
    // check if subcore is back to idle
    gcore_subcore_idle();
    return;

}

/*
 * Loads subcore, agent and dutcore to proxy the FSM state to 
 * load into memcore. Runs the state after loading it.
 *
 */
void helper_memcore_load_run(enum artix_selects artix_select,
    struct gcore_ctrl_packet *packet, uint32_t num_bursts){
    enum dutcore_states dutcore_state;
    struct gcore_ctrl_packet status_packet;

    if(packet == NULL){
        die("error: pointer is NULL\n");
    }
    print_packet(packet, "memcore: ");

    // save memcore state which we'll load
    enum memcore_states loading_state = (packet->data & 0x0000000f);

    dutcore_state = MEM_LOAD;
    helper_dutcore_load_run(artix_select, dutcore_state);
    
    helper_print_agent_status(artix_select);
    
    // check if dutcore is in mem_load state
    helper_get_agent_status(artix_select, &status_packet);
    if((status_packet.data & 0x000000f0) != 0x00000030){
        printf("error: dutcore is not in MEM_LOAD state. 0x%08X\n", status_packet.data);
        exit(1);
    }

    // send the packet to MEM_LOAD
    helper_dutcore_packet_write(artix_select, packet);
    
    // run the loaded state
    dutcore_state = MEM_RUN;
    helper_dutcore_load_run(artix_select, dutcore_state);
    
    // memcore is reading into a fifo so if the number of bursts 
    // read is less than the fifos capacity it will exit
    if((loading_state == MEMCORE_READ_BURST && (num_bursts*BURST_BYTES) > ARTIX_READ_FIFO_BYTES)
        || loading_state != MEMCORE_READ_BURST) {
        helper_get_agent_status(artix_select, &status_packet);
        if((status_packet.data & 0x00000f00) != (loading_state << 8)){
            printf("error: memcore is not in desired state. desired: 0x%08X actual: 0x%08X\n", 
                (loading_state << 8), (status_packet.data & 0x00000f00));
            exit(1);
        }
    }

    return;
}

/* 
 * Loads dutcore with given fsm state through agent and
 * through subcore, then checks if subcore is idle.
 *
 */
void helper_dutcore_load_run(enum artix_selects artix_select,
        enum dutcore_states dutcore_state){
    struct gcore_ctrl_packet packet;
    enum agent_states agent_state;

    // fill packet
    packet.rank_select = 0;
    packet.addr = 0;
    packet.data = dutcore_state;
    print_dutcore_state(dutcore_state, "dutcore: ");

    helper_dutcore_packet_write(artix_select, &packet);
    
    agent_state = DUT_RUN;
    helper_agent_load_run(artix_select, agent_state);
    
    // wait for subcore idle state
    gcore_subcore_idle();
    
    return;
}

/*
 * Performs a packet write through the ctrl axi-lite registers
 * from a gcore packet, through subcore and agent to dutcore.
 *
 */
void helper_dutcore_packet_write(enum artix_selects artix_select,
        struct gcore_ctrl_packet *packet){
    enum agent_states agent_state;
    enum subcore_states subcore_state;

    // set agent to proxy data
    agent_state = DUT_LOAD;
    helper_agent_load_run(artix_select, agent_state);
    
    // set subcore to proxy ctrl data
    subcore_state = CTRL_WRITE;
    helper_subcore_load_run(artix_select, subcore_state);
    
    // write ctrl_axi
    gcore_ctrl_write(packet);
    
    // wait for subcore idle state
    gcore_subcore_idle();
    
    return;
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
    // clean packet
    packet->rank_select = 0;
    packet->addr = 0;
    packet->data = 0;

    enum agent_states agent_state = STATUS;
    helper_agent_load_run(artix_select, agent_state);
    
	// agent is in status, do ctrl_read to grab data
    enum subcore_states subcore_state = CTRL_READ;
    helper_subcore_load_run(artix_select, subcore_state);
    
    // read ctrl_axi
    gcore_ctrl_read(packet);
    
    // wait for subcore idle state
    gcore_subcore_idle();
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
    print_packet(&packet, "status: ");
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
    printf("control: 0x%08X\n", regs->control);
    printf("status: 0x%08X\n", regs->status);
    printf("addr: 0x%08X\n", regs->addr);
    printf("data: 0x%08X\n", regs->data);
    printf("a1_status: 0x%08X\n", regs->a1_status);
    printf("a2_status: 0x%08X\n", regs->a2_status);
    return;
}

/*
 * Prints a gcore ctrl packet to stdout.
 *
 */
void print_packet(struct gcore_ctrl_packet *packet, char *pre){
    if(packet == NULL){
        die("error: pointer is NULL\n");
    }
    if(pre == NULL){
        die("error: pointer is NULL\n");
    }
    printf(
        "%srank_sel: %d, addr: 0x%08X, data: 0x%08X\n",
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
        die("error: pointer is NULL\n");
    }

    gcore_subcore_mode_state(&mode_state);
    
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
    };

    sprintf(mode_state_str, "subcore: mode=%s state=%s\n", mode_str, state_str);

    return;
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
            printf("%sagent_init\n", pre);
            break;
        case AGENT_IDLE:
            printf("%sagent_idle\n", pre);
            break;
        case AGENT_PAUSED:
            printf("%sagent_paused\n", pre);
            break;
        case STATUS:
            printf("%sstatus\n", pre);
            break;
        case BURST_LOAD:
            printf("%sburst_load\n", pre);
            break;
        case DUT_LOAD:
            printf("%sdut_load\n", pre);
            break;
        case DUT_RUN:
            printf("%sdut_run\n", pre);
            break;
        case DUT_WRITE:
            printf("%sdut_write\n", pre);
            break;
        case DUT_READ:
            printf("%sdut_read\n", pre);
            break;
        case DUT_RESET:
            printf("%sdut_reset\n", pre);
            break;
        default:
            break;
    };
    return;
}

/*
 * Given a dutcore state enum, print the state name to stdout.
 *
 */
void print_dutcore_state(enum dutcore_states dutcore_state, char *pre){
    if(pre == NULL){
        return;
    }
    switch(dutcore_state){
        case DUTCORE_IDLE:
            printf("%sdutcore_idls\n", pre);
			break;
        case DUTCORE_PAUSED:
            printf("%sdutcore_paused\n", pre);
			break;
        case MEM_BURST:
            printf("%smem_burst\n", pre);
			break;
        case MEM_LOAD:
            printf("%smem_load\n", pre);
			break;
        case MEM_RUN:
            printf("%smem_run\n", pre);
			break;
        case MEM_WRITE:
            printf("%smem_write\n", pre);
			break;
        case MEM_READ:
            printf("%smem_read\n", pre);
			break;
        case MEM_TEST:
            printf("%smem_test\n", pre);
			break;
        case TEST_INIT:
            printf("%stest_init\n", pre);
			break;
        case TEST_SETUP:
            printf("%stest_setup\n", pre);
			break;
        case TEST_RUN:
            printf("%stest_run\n", pre);
			break;
        case TEST_CLEANUP:
            printf("%stest_cleanup\n", pre);
			break;
        default:
            break;
    }
    return;
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
            printf("%smemcore_idle\n", pre);
			break;
        case MEMCORE_PAUSED:
            printf("%smemcore_paused\n", pre);
			break;
        case MEMCORE_AUTO_BURST:
            printf("%smemcore_auto_burst\n", pre);
			break;
        case MEMCORE_WRITE_BURST:
            printf("%smemcore_write_burst\n", pre);
			break;
        case MEMCORE_READ_BURST:
            printf("%smemcore_read_burst\n", pre);
			break;
        case MEMCORE_CLEANUP:
            printf("%smemcore_cleanup\n", pre);
			break;
        default:
            break;
    }
    return;
}

