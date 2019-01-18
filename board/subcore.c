/*
 * Subcore interface through Gemini Driver ioctl calls.
 *
 */

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "../common.h"
#include "../util.h"
#include "dev.h"
#include "subcore.h"

#include "../../driver/gcore_common.h"

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

/*
 * Configure subcore with an FSM state and artix unit.
 */
void gcore_subcore_load(struct gcore_cfg *gcfg){
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_LOAD, gcfg) < 0){
		die("gcorelib: error subcore_load failed");
	}
    return;
}

/*
 * Run loaded state.
 */
void gcore_subcore_run(){
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_RUN) < 0){
		die("gcorelib: error subcore_run failed");
	}
    return;
}

/*
 * Waits for subcore to go back to IDLE state.
 */
void gcore_subcore_idle(){
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    // wait for done to go high
    if(ioctl(gcore_fd, GCORE_SUBCORE_IDLE) < 0){
		die("gcorelib: error subcore_idle failed");
	}
    return;
}

/*
 * Return the current state of subcore.
 *
 */
void gcore_subcore_mode_state(uint32_t *mode_state){
    struct gcore_registers *regs;
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_STATE) < 0){
		die("gcorelib: error subcore_state failed");
	}
    regs = gcore_get_regs();
    (*mode_state) = (uint32_t)regs->data;
    regs = gcore_free_regs(regs);
    return;
}

/*
 * Peform a subcore soft reset.
 */
void gcore_subcore_reset(){
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_SUBCORE_RESET) < 0){
		die("gcorelib: error subcore_reset failed");
	}
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
void gcore_ctrl_write(struct gcore_ctrl_packet *packet){
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_CTRL_WRITE, packet) < 0){
		die("gcorelib: error ctrl_write failed");
	}
    return;
}

/*
 * Subcore must be in ctrl_read state. Calling this ioctl
 * will read from subcore into the rank_sel, addr, data.
 */
void gcore_ctrl_read(struct gcore_ctrl_packet *packet){
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_CTRL_READ, packet) < 0){
		die("gcorelib: error ctrl_read failed");
	}
    return;
}

/*
 * Asserts the artix sync line. Do this if running duttest
 * in dual mode which will force the gvpus to wait for each
 * other before starting the dut test.
 *
 */
void gcore_artix_sync(bool sync){
    int gcore_fd = -1;
    struct gcore_ctrl_packet packet;

    packet.rank_select = 0;
    packet.addr = 0x00000000;
    if(sync){
        packet.data = 0x00000001;
    }else{
        packet.data = 0x00000000;
    }
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if(ioctl(gcore_fd, GCORE_ARTIX_SYNC, &packet) < 0){
		die("gcorelib: error artix_sync failed");
	}
    return;
}

/*
 * Gets all values of the registers.
 */
struct gcore_registers* gcore_get_regs(){
    struct gcore_registers *regs = NULL;
    int gcore_fd = -1;
    if((gcore_fd = gcore_dev_get_fd()) == -1){
        die("failed to get gcore fd");
    }
    if((regs = (struct gcore_registers *) malloc(sizeof(struct gcore_registers))) == NULL){
        die("error: malloc failed");
    }
    regs->control = (u32) 0;
	regs->status = (u32) 0;
	regs->addr = (u32) 0;
	regs->data = (u32) 0;
	regs->a1_status = (u32) 0;
	regs->a2_status = (u32) 0;
    if(ioctl(gcore_fd, GCORE_REGS_READ, regs) < 0){
		die("gcorelib: error regs_read failed");
	}
    return regs;
}

/*
 * Free the allocated regs.
 *
 */
struct gcore_registers *gcore_free_regs(struct gcore_registers *regs){
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



