/*
 * Subcore interface through Gemini Driver ioctl calls.
 *
 */

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "common.h"
#include "subcore.h"
#include "util.h"

#include "../driver/gemini_core.h"

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

static int gcore_fd;
static bool is_initialized = false;

__attribute__((constructor))
static void gcore_subcore_init() {
    gcore_fd = open(MMAP_PATH, O_RDONLY, 0);
	if(gcore_fd == -1){
		die("gcorelib: opening file for writing");
    }
    is_initialized = true;
    return;
}

__attribute__((destructor))
static void gcore_subcore_destroy() {
    if(!is_initialized){
        die("gcorelib: failed to exit, gcore not initialized");
    }
	close(gcore_fd);
    is_initialized = false;
	return;
}

/*
 * Configure subcore with an FSM state and artix unit.
 */
void gcore_subcore_load(struct gcore_cfg *gcfg){
    if(ioctl(gcore_fd, GCORE_SUBCORE_LOAD, gcfg) < 0){
		die("gcorelib: error subcore_load failed");
	}
    return;
}

/*
 * Run loaded state.
 */
void gcore_subcore_run(){
    if(ioctl(gcore_fd, GCORE_SUBCORE_RUN) < 0){
		die("gcorelib: error subcore_run failed");
	}
    return;
}

/*
 * Waits for subcore to go back to IDLE state.
 */
void gcore_subcore_idle(){
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
    if(ioctl(gcore_fd, GCORE_SUBCORE_STATE) < 0){
		die("gcorelib: error subcore_state failed");
	}
    regs = gcore_get_regs();
    (*mode_state) = (uint32_t)regs->data;
    return;
}

/*
 * Peform a subcore soft reset.
 */
void gcore_subcore_reset(){
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
    if(ioctl(gcore_fd, GCORE_CTRL_READ, packet) < 0){
		die("gcorelib: error ctrl_read failed");
	}
    return;
}

