#include "libgcore.h"

// can't include kernel headers so add defines here
#define u32 uint32_t
#define dma_cookie_t int32_t

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>

#define BUS_IN_BYTES 8
#define BUS_BURST 16

// AXI DMA register offsets
#define DMA_BASE_ADDR 0x40400000
#define MM2S_CONTROL 0x00
#define MM2S_STATUS 0x04
#define MM2S_CURDESC 0x08
#define MM2S_CURDESC_MSB 0x0C
#define MM2S_TAILDESC 0x10
#define MM2S_TAILDESC_MSB 0x14
#define MM2S_START_ADDR 0x18
#define MM2S_LENGTH 0x28
#define S2MM_CONTROL 0x30
#define S2MM_STATUS 0x34
#define S2MM_CURDESC 0x38
#define S2MM_CURDESC_MSB 0x3C
#define S2MM_TAILDESC 0x40
#define S2MM_TAILDESC_MSB 0x44
#define S2MM_DEST_ADDR 0x48
#define S2MM_LENGTH 0x58

static uint32_t alloc_offset;
static int fd;
static uint8_t *map;
static int mfd;
static uint32_t *dma_map;
struct gcore_userdev userdev;
struct gcore_chan_cfg rx_config;
struct gcore_chan_cfg tx_config;
static bool is_initialized = false;
static bool is_rx_prepared = false;
static bool is_tx_prepared = false;

/* ==========================================================================
 * Initialization
 * ==========================================================================
 */

/*
 * mmap to /dev/gcore, reset alloc buffer, get userdevs
 */
int gcore_init(){
    int status = 0;

	fd = open(MMAP_PATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if (fd == -1) {
		perror("gcorelib: opening file for writing");
		return EXIT_FAILURE;
	}
	
    map = mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		perror("gcorelib: mmapping the file");
		return EXIT_FAILURE;
	}


	gcore_dma_alloc_reset();

    userdev.tx_chan = (u32) 0;
    userdev.tx_cmp = (u32) 0;
    userdev.rx_chan = (u32) 0;
    userdev.rx_cmp = (u32) 0;

    if (ioctl(fd, GCORE_USERDEVS_READ, &userdev) < 0) {
		perror("gcorelib: error userdevs_read failed");
		status = errno;
    }

    // mmap the axi lite register block
    mfd = open("/dev/mem", O_RDONLY); 
    dma_map = mmap(NULL, 65535, PROT_READ, MAP_SHARED, mfd, DMA_BASE_ADDR);

    is_initialized = true;
    return status;
}

/*
 * Do some cleanup.
 */
int gcore_exit(void){
    if(!is_initialized) {
        perror("gcorelib: failed to exit, gcore not initialized");
		return EXIT_FAILURE;
    }

	if (munmap(map, MMAP_SIZE) == -1) {
		perror("gcorelib: error un-mmapping the file");
		return EXIT_FAILURE;
	}
	close(fd);

    if (munmap(dma_map, 65535) == -1) {
		perror("gcorelib: error un-mmapping the file");
		return EXIT_FAILURE;
	}
	close(mfd);

    is_initialized = false;
	return EXIT_SUCCESS;
}


/* ==========================================================================
 * Subcore
 * ==========================================================================
 */

/*
 * Configure subcore with an FSM state and artix unit.
 */
int gcore_subcore_load(struct gcore_cfg *gcfg) {
    int status = 0;
    if(ioctl(fd, GCORE_SUBCORE_LOAD, gcfg) < 0) {
		perror("gcorelib: error subcore_load failed");
		status = errno;
	}
    return status;
}

/*
 * Run loaded state.
 */
int gcore_subcore_run() {
    int status = 0;
    if(ioctl(fd, GCORE_SUBCORE_RUN) < 0) {
		perror("gcorelib: error subcore_run failed");
		status = errno;
	}
    return status;
}

/*
 * Waits for subcore to go back to IDLE state.
 */
int gcore_subcore_idle() {
    int status = 0;
    // wait for done to go high
    if(ioctl(fd, GCORE_SUBCORE_IDLE) < 0) {
		perror("gcorelib: error subcore_idle failed");
		status = errno;
	}
    return status;
}

/*
 * Return the current state of subcore.
 *
 */
int gcore_subcore_mode_state(uint32_t *mode_state) {
    int status = 0;
    struct gcore_registers *regs;
    if(ioctl(fd, GCORE_SUBCORE_STATE) < 0) {
		perror("gcorelib: error subcore_state failed");
		status = errno;
	}
    regs = gcore_get_regs();
    (*mode_state) = (uint32_t)regs->data;
    return status;
}

/*
 * Peform a subcore soft reset.
 */
int gcore_subcore_reset() {
    int status = 0;
    if(ioctl(fd, GCORE_SUBCORE_RESET) < 0) {
		perror("gcorelib: error subcore_reset failed");
		status = errno;
	}
    return status;
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
int gcore_ctrl_write(struct gcore_ctrl_packet *packet){
    int s = 0;
    if(ioctl(fd, GCORE_CTRL_WRITE, packet) < 0) {
		perror("gcorelib: error ctrl_write failed");
		s = errno;
	}
    return s;
}

/*
 * Subcore must be in ctrl_read state. Calling this ioctl
 * will read from subcore into the rank_sel, addr, data.
 */
int gcore_ctrl_read(struct gcore_ctrl_packet *packet){
    int s = 0;
    if(ioctl(fd, GCORE_CTRL_READ, packet) < 0) {
		perror("gcorelib: error ctrl_read failed");
		s = errno;
	}
    return s;
}

/* ==========================================================================
 * DMA
 * ==========================================================================
 */

uint32_t gcore_dma_calc_offset(void *ptr){
	return (((uint8_t *) ptr) - &map[0]);
}

uint32_t gcore_dma_calc_size(int length, int byte_num) {
	const int block = (BUS_IN_BYTES * BUS_BURST);
	length = length * byte_num;

	if (0 != (length % block)) {
		length += (block - (length % block));
	}

	return length;
}

void *gcore_dma_alloc(int length, int byte_num){
	void *array = &map[alloc_offset];
	alloc_offset += gcore_dma_calc_size(length, byte_num);
	return array;
}

void gcore_dma_alloc_reset(void){
	alloc_offset = 0;
}

/* Perform DMA transaction
 *
 * To perform a one-way transaction set the unused directions pointer to NULL
 * or length to zero.
 */
int gcore_dma_prep( uint64_t *tx_ptr, size_t tx_size,
    uint64_t *rx_ptr, size_t rx_size)
{
	int ret = 0;
	const bool tx_used = ((tx_ptr != NULL) && (tx_size != 0));
	const bool rx_used = ((rx_ptr != NULL) && (rx_size != 0));
    
    if(!is_initialized) {
        perror("gcorelib: failed to dma prep, gcore not initialized");
		return EXIT_FAILURE;
    }

    if (tx_used) {
        tx_config.chan = userdev.tx_chan;
        tx_config.buf_offset = (u32) gcore_dma_calc_offset(tx_ptr);
        tx_config.buf_size = (u32) tx_size;
        tx_config.dir = GCORE_MEM_TO_DEV;
        if (ioctl(fd, GCORE_DMA_CONFIG, &tx_config) < 0) {
            perror("gcorelib: error config dma tx chan");
            return EXIT_FAILURE;
        }
    }

	if (rx_used) {
        rx_config.chan = userdev.rx_chan;
        rx_config.buf_offset = (u32) gcore_dma_calc_offset(rx_ptr);
        rx_config.buf_size = (u32) rx_size;
        rx_config.dir = GCORE_DEV_TO_MEM;
        if (ioctl(fd, GCORE_DMA_CONFIG, &rx_config) < 0) {
            perror("gcorelib: error config dma rx chan");
            return EXIT_FAILURE;
        }
    }

	if (tx_used) {
		tx_config.completion = userdev.tx_cmp;
		ret = (int)ioctl(fd, GCORE_DMA_PREP, &tx_config);
		if (ret < 0) {
			perror("gcorelib: error prep dma tx buf");
			return ret;
		}
	}

	if (rx_used) {
		rx_config.completion = userdev.rx_cmp;
		ret = (int)ioctl(fd, GCORE_DMA_PREP, &rx_config);
		if (ret < 0) {
			perror("gcorelib: error prep dma rx buf");
			return ret;
		}
	}
    
    if (tx_used) {
        is_tx_prepared = true;
    }else {
        is_tx_prepared = false;
    }

	if (rx_used) {
        is_rx_prepared = true;
    } else {
        is_rx_prepared = false;
    }

    return ret;
}

/* Perform DMA transaction
 *
 * To perform a one-way transaction set the unused directions pointer to NULL
 * or length to zero.
 */
int gcore_dma_start(enum gcore_wait wait)
{   
    int ret = 0;
	struct gcore_transfer rx_trans;
	struct gcore_transfer tx_trans;

    if(!is_initialized) {
        perror("gcorelib: failed to start dma, gcore not initialized");
		return EXIT_FAILURE;
    }

    if(!(is_tx_prepared || is_rx_prepared)) {
        perror("gcorelib: error starting dma, not prepared yet");
        return -1;
    }

	if (is_tx_prepared) {
		tx_trans.chan = userdev.tx_chan;
		tx_trans.wait = (0 != (wait & GCORE_WAIT_TX));
		tx_trans.wait_time_msecs = 3000;
        tx_trans.completion = userdev.tx_cmp;
		tx_trans.cookie = tx_config.cookie;
        tx_trans.buf_size = tx_config.buf_size;
		
        ret = (int)ioctl(fd, GCORE_DMA_START, &tx_trans);
		if (ret < 0) {
			perror("gcorelib: error starting dma tx transaction");
			return ret;
		}

        // reset flag
        is_tx_prepared = false;
	}

	if (is_rx_prepared) {
		rx_trans.chan = userdev.rx_chan;
		rx_trans.wait = (0 != (wait & GCORE_WAIT_RX));
		rx_trans.wait_time_msecs = 3000;
		rx_trans.completion = userdev.rx_cmp;
		rx_trans.cookie = rx_config.cookie;
        rx_trans.buf_size = rx_config.buf_size;

		ret = (int)ioctl(fd, GCORE_DMA_START, &rx_trans);
		if (ret < 0) {
			perror("gcorelib: error starting dma rx transaction");
			return ret;
		}
        
        // reset flag
        is_rx_prepared = false;
	}

	return ret;
}

int gcore_dma_stop()
{
	int ret = 0;
	struct gcore_transfer rx_trans;
	struct gcore_transfer tx_trans;

    if(!is_initialized) {
        perror("gcorelib: failed to stop dma, gcore not initialized");
		return EXIT_FAILURE;
    }

    if(!(is_tx_prepared || is_rx_prepared)) {
        perror("gcorelib: error failed to stop dma, nothing is running");
        return -1;
    }

	if (is_tx_prepared) {
		tx_trans.chan = userdev.tx_chan;
		ret = (int)ioctl(fd, GCORE_DMA_STOP, &(tx_trans.chan));
		if (ret < 0) {
			perror("gcorelib: error stopping dma tx trans");
			return ret;
		}
        is_tx_prepared = false;
	}

	if (is_rx_prepared) {
		rx_trans.chan = userdev.rx_chan;
		ret = (int)ioctl(fd, GCORE_DMA_STOP, &(rx_trans.chan));
		if (ret < 0) {
			perror("gcorelib: error stopping dma rx trans");
			return ret;
		}
        is_rx_prepared = false;
	}
	return ret;
}

/* ==========================================================================
 * AXI DMA IP 
 * ==========================================================================
 */

uint32_t dma_get(uint32_t offset) {
    return ((uint32_t*)dma_map)[offset>>2];
}

void dma_status_reg(uint32_t status) {
    if (status & (1 << 0)) printf(" halted"); else printf(" running");
    if (status & (1 << 1)) printf(" idle");
    if (status & (1 << 3)) printf(" SGIncld");
    if (status & (1 << 4)) printf(" DMAIntErr");
    if (status & (1 << 5)) printf(" DMASlvErr");
    if (status & (1 << 6)) printf(" DMADecErr");
    if (status & (1 << 8)) printf(" SGIntErr");
    if (status & (1 << 9)) printf(" SGSlvErr");
    if (status & (1 << 10)) printf(" SGDecErr");
    if (status & (1 << 12)) printf(" IOC_Irq");
    if (status & (1 << 13)) printf(" Dly_Irq");
    if (status & (1 << 14)) printf(" Err_Irq");
    printf(" IRQThresholdSts (0x%02x)", (status & (0xff << 16)) >> 16);
    printf(" IRQDelaySts (0x%02x)", (status & (0xff << 24)) >> 24);
    printf("\n");
    return;
}

void dma_control_reg(uint32_t control) {
    if (control & (1 << 0)) printf(" run"); else printf(" stop");
    if (control & (1 << 2)) printf(" reset");
    if (control & (1 << 3)) printf(" Keyhole");
    if (control & (1 << 4)) printf(" CyclicBD");
    if (control & (1 << 12)) printf(" IOC_IrqEn");
    if (control & (1 << 13)) printf(" Dly_IrqEn");
    if (control & (1 << 14)) printf(" Err_IrqEn");
    printf(" IRQThreshold (0x%02x)", (control & (0xff << 16)) >> 16);
    printf(" IRQDelay (0x%02x)", (control & (0xff << 24)) >> 24);
    printf("\n");
    return;
}

void dma_mm2s_status() {
    uint32_t status = 0;
    if(!is_initialized) {
        perror("gcorelib: failed to get mm2s status, gcore not initialized");
        return;
    }
    status = dma_get(MM2S_CONTROL);
    printf("MM2S control (0x%08x@0x%02x):", status, MM2S_CONTROL);
    dma_control_reg(status);
    status = dma_get(MM2S_STATUS);
    printf("MM2S status (0x%08x@0x%02x):", status, MM2S_STATUS);
    dma_status_reg(status);
    printf("MM2S curdesc: 0x%08x@0x%02x\n", dma_get(MM2S_CURDESC) >> 6, MM2S_CURDESC);
    //printf("MM2S curdesc_msb: 0x%08x@0x%02x\n", dma_get(MM2S_CURDESC_MSB), MM2S_CURDESC_MSB);
    printf("MM2S taildesc: 0x%08x@0x%02x\n", dma_get(MM2S_TAILDESC) >> 6, MM2S_TAILDESC);
    //printf("MM2S taildesc_msb: 0x%08x@0x%02x\n", dma_get(MM2S_TAILDESC_MSB), MM2S_TAILDESC_MSB);
    printf("MM2S start_addr: 0x%08x@0x%02x\n", dma_get(MM2S_START_ADDR), MM2S_START_ADDR);
    printf("MM2S length: 0x%08x@0x%02x\n", dma_get(MM2S_LENGTH), MM2S_LENGTH);
    return;
}

void dma_s2mm_status() {
    uint32_t status = 0;
    if(!is_initialized) {
        perror("gcorelib: failed to get s2mm status, gcore not initialized");
        return;
    }
    status = dma_get(S2MM_CONTROL);
    printf("S2MM control (0x%08x@0x%02x):", status, S2MM_CONTROL);
    dma_control_reg(status);
    status = dma_get(S2MM_STATUS);
    printf("S2MM status (0x%08x@0x%02x):", status, S2MM_STATUS);
    dma_status_reg(status);
    printf("S2MM curdesc: 0x%08x@0x%02x\n", dma_get(S2MM_CURDESC) >> 6, S2MM_CURDESC);
    //printf("S2MM curdesc_msb: 0x%08x@0x%02x\n", dma_get(S2MM_CURDESC_MSB), S2MM_CURDESC_MSB);
    printf("S2MM taildesc: 0x%08x@0x%02x\n", dma_get(S2MM_TAILDESC) >> 6, S2MM_TAILDESC);
    //printf("S2MM taildesc_msb: 0x%08x@0x%02x\n", dma_get(S2MM_TAILDESC_MSB), S2MM_TAILDESC_MSB);
    printf("S2MM start_addr: 0x%08x@0x%02x\n", dma_get(S2MM_DEST_ADDR), S2MM_DEST_ADDR);
    printf("S2MM length: 0x%08x@0x%02x\n", dma_get(S2MM_LENGTH), S2MM_LENGTH);
    return;
}

void dma_status(){
    dma_mm2s_status();
    dma_s2mm_status();
    return;
}

/* ==========================================================================
 * Helper
 * ==========================================================================
 */

/*
 * Gets all values of the registers.
 */
struct gcore_registers* gcore_get_regs(){
    struct gcore_registers *regs;

    if((regs = (struct gcore_registers *)
        malloc(sizeof(struct gcore_registers))) == NULL){
        return NULL;
    }
    regs->control = (u32) 0;
	regs->status = (u32) 0;
	regs->addr = (u32) 0;
	regs->data = (u32) 0;
	regs->a1_status = (u32) 0;
	regs->a2_status = (u32) 0;
    if (ioctl(fd, GCORE_REGS_READ, regs) < 0) {
		perror("gcorelib: error regs_read failed");
		exit(EXIT_FAILURE);
	}
    return regs;
}

/*
 * helper function to prepare and start dma.
 */
int gcore_dma_prep_start(enum gcore_wait wait,
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size){

    int s = 0;
    s = gcore_dma_prep(tx_ptr, tx_size, rx_ptr, rx_size);	
    if(s){ goto error; }
    s = gcore_dma_start(wait);	
    if(s){ goto error; }

error:
    return s;
}

