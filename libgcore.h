#ifndef LIBGCORE_H
#define LIBGCORE_H

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../driver/gemini_core.h"

#ifndef die
#define die(fmt, ...) do{ fprintf(stderr, "%s:%d:%s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); exit(EXIT_FAILURE); } while(0);
#endif

#define MMAP_PATH "/dev/gcore"
#define MMAP_SIZE (DMA_SIZE * sizeof(uint8_t))

enum gcore_wait {
    GCORE_WAIT_NONE = 0,
    GCORE_WAIT_TX = (1 << 0),
    GCORE_WAIT_RX = (1 << 1),
    GCORE_WAIT_BOTH = (1 << 1) | (1 << 0),
};

/*
 * Initialization
 */
void gcore_init();
void gcore_exit();

/*
 * Subcore
 */
void gcore_subcore_load(struct gcore_cfg* gcfg);
void gcore_subcore_run();
void gcore_subcore_idle();
void gcore_subcore_mode_state(uint32_t *mode_state);
void gcore_subcore_reset();

/*
 * Crtl Axi
 */

void gcore_ctrl_write(struct gcore_ctrl_packet *packet);
void gcore_ctrl_read(struct gcore_ctrl_packet *packet);

/*
 * DMA
 */
void *gcore_dma_alloc(int length, int byte_num);
void gcore_dma_alloc_reset(void);
void gcore_dma_prep(
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);
void gcore_dma_start(enum gcore_wait wait);
void gcore_dma_stop();

/*
 * Direct access to axi dma registers.
 *
 */

void dma_mm2s_status();
void dma_s2mm_status();
void dma_status();

/*
 * Access the MIO/EMIO gpio through the sysfs interface.
 */

#define GCORE_GPIO_MIN 0
#define GCORE_GPIO_MAX 117
#define GCORE_GPIO_START 906

enum gcore_gpio_dir {
    GCORE_GPIO_NONE,
    GCORE_GPIO_READ,
    GCORE_GPIO_WRITE
};

bool gcore_gpio(uint32_t index, enum gcore_gpio_dir dir, bool value);

/*
 * I2C functions
 */

// TODO: the ddr eeprom addr might change depending
// on the sodimm.

#define GCORE_I2C_SWITCH_ADDR (0x76)
#define GCORE_I2C_Z_EEPROM_ADDR_0 (0x54)
#define GCORE_I2C_Z_EEPROM_ADDR_1 (0x55)
#define GCORE_I2C_Z_EEPROM_ADDR_2 (0x56)
#define GCORE_I2C_Z_EEPROM_ADDR_3 (0x57)
#define GCORE_I2C_A_DDR_EEPROM_ADDR (0x50)
#define GCORE_I2C_USRCLK_ADDR (0x55) 

enum gcore_i2c_devs {
    GCORE_I2C_DEV_Z_EEPROM=0x00000001,
    GCORE_I2C_DEV_A1_DDR_EEPROM=0x00000002,
    GCORE_I2C_DEV_A2_DDR_EEPROM=0x00000004,
    GCORE_I2C_DEV_Z_USRCLK=0x00000008,
    GCORE_I2C_DEV_A1_USRCLK=0x00000010,
    GCORE_I2C_DEV_A2_USRCLK=0x00000020,
    GCORE_I2C_DEV_Z_EXT0=0x00000040,
    GCORE_I2C_DEV_Z_EXT1=0x00000080
};

enum gcore_i2c_dir {
    GCORE_I2C_NONE,
    GCORE_I2C_READ,
    GCORE_I2C_WRITE
};


uint8_t gcore_i2c(uint8_t addr, uint8_t reg, enum gcore_i2c_dir dir, uint8_t value);
void gcore_i2c_set_dev(enum gcore_i2c_devs dev);


/*
 * Helper
 */
struct gcore_registers *gcore_get_regs();
void gcore_dma_prep_start(enum gcore_wait wait,
     uint64_t *tx_ptr, size_t tx_size,
     uint64_t *rx_ptr, size_t rx_size);   

#ifdef __cplusplus
}
#endif
#endif				/* LIBGCORE_H */
