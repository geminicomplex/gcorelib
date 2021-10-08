#ifndef I2C_H
#define I2C_H

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../driver/gcore_common.h"

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

#ifdef __cplusplus
}
#endif
#endif				/* I2C_H */
