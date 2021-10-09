/*
 * Subcore interface through gcore ioctl calls.
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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>

#include "../common.h"
#include "../util.h"
#include "dev.h"
#include "subcore.h"
#include "driver.h"

#ifdef VERILATOR
#include "../../sim/chip_top/chip.h"
#endif

/*
 * Configure subcore with an FSM state and artix unit.
 */
void subcore_load(struct gcore_cfg *gcfg){
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_load(chip, gcfg->artix_select, gcfg->subcore_state);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_LOAD, gcfg) < 0){
		die("gcorelib: error subcore_load failed");
	}
#endif
    return;
}

/*
 * Run loaded state.
 */
void subcore_run(){
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_run(chip);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_RUN) < 0){
		die("gcorelib: error subcore_run failed");
	}
#endif
    return;
}

/*
 * Waits for subcore to go back to IDLE state.
 */
void subcore_idle(){
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_idle(chip);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    // wait for done to go high
    if(ioctl(gcore_fd, GCORE_SUBCORE_IDLE) < 0){
		die("gcorelib: error subcore_idle failed");
	}
#endif
    return;
}

/*
 * Return the current state of subcore.
 *
 */
void subcore_mode_state(uint32_t *mode_state){
    struct gcore_registers *regs;
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_state(chip);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_STATE) < 0){
		die("gcorelib: error subcore_state failed");
	}
#endif
    regs = subcore_get_regs();
    (*mode_state) = (uint32_t)regs->data;
    regs = subcore_free_regs(regs);
    return;
}

/*
 * Peform a subcore soft reset.
 */
void subcore_reset(){
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_reset(chip);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_RESET) < 0){
		die("gcorelib: error subcore_reset failed");
	}
#endif
    return;
}

/* ==========================================================================
 * CTRL
 * ==========================================================================
 */

/*
 * Subcore msut be in ctrl_write state. Calling this ioctl
 * will write to subcore what's in the rank_sel, addr
 * and data regs.
 */
void subcore_write_packet(struct gcore_ctrl_packet *packet){
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_ctrl_write(chip, packet);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_CTRL_WRITE, packet) < 0){
		die("gcorelib: error ctrl_write failed");
	}
#endif
    return;
}

/*
 * Subcore must be in ctrl_read state. Calling this ioctl
 * will read from subcore into the rank_sel, addr, data.
 */
void subcore_read_packet(struct gcore_ctrl_packet *packet){
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_ctrl_read(chip, packet);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_CTRL_READ, packet) < 0){
		die("gcorelib: error ctrl_read failed");
	}
#endif
    return;
}

/*
 * Asserts the artix sync line. Do this if running duttest
 * in dual mode which will force the gvpus to wait for each
 * other before starting the dut test.
 *
 */
void subcore_artix_sync(bool sync){
    int gcore_fd = -1;
    struct gcore_ctrl_packet packet;

    packet.rank_select = 0;
    packet.addr = 0x00000000;
    if(sync){
        packet.data = 0x00000001;
    }else{
        packet.data = 0x00000000;
    }
#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_subcore_artix_sync(chip, &packet);
#else
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_ARTIX_SYNC, &packet) < 0){
		die("gcorelib: error artix_sync failed");
	}
#endif
    return;
}

/*
 * Gets all values of the registers.
 */
struct gcore_registers* subcore_get_regs(){
    struct gcore_registers *regs = NULL;
    if((regs = (struct gcore_registers *) malloc(sizeof(struct gcore_registers))) == NULL){
        die("error: malloc failed");
    }
    regs->control = (u32) 0;
	regs->status = (u32) 0;
	regs->addr = (u32) 0;
	regs->data = (u32) 0;
	regs->a1_status = (u32) 0;
	regs->a2_status = (u32) 0;

#ifdef VERILATOR
    struct chip *chip = get_chip_instance();
    sim_ioctl_regs_read(chip, regs);
#else
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    
    if(ioctl(gcore_fd, GCORE_REGS_READ, regs) < 0){
		die("gcorelib: error regs_read failed");
	}
#endif
    return regs;
}

/*
 * Free the allocated regs.
 *
 */
struct gcore_registers *subcore_free_regs(struct gcore_registers *regs){
    if(regs == NULL){
        die("pointer is NULL");
    }

    regs->control = (u32) 0;
	regs->status = (u32) 0;
	regs->addr = (u32) 0;
	regs->data = (u32) 0;
	regs->a1_status = (u32) 0;
	regs->a2_status = (u32) 0;
    free(regs);

    return NULL;
}



