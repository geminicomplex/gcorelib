/*
 * i2c helper functions
 *
 */


// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "../common.h"
#include "i2c.h"

#include "../../driver/gcore_common.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <linux/i2c-dev.h>

/*
 * Perform a raw i2c read or write given an addr and reg.
 */
uint8_t gcore_i2c(uint8_t addr, uint8_t reg, enum gcore_i2c_dir dir, uint8_t value){
    int32_t fd = -1;
    uint32_t adapter = 0;
    char filename[20];
    uint8_t buf[10];

    snprintf(filename, 19, "/dev/i2c-%d", adapter);
    if((fd = open(filename, O_RDWR)) < 0){
        fprintf(stderr, "failed to open i2c dev '/dev/i2c-%d'.", adapter);
        exit(1);
    }

    if(ioctl(fd, I2C_SLAVE, addr) < 0){
        fprintf(stderr, "failed to do ioctl I2C_SLAVE for addr %i", addr);
        exit(1);
    }

    if(dir == GCORE_I2C_READ){
        buf[0] = reg;
        if(write(fd, buf, 1) != 1){
            fprintf(stderr, "failed to prep read for addr %i and reg %i\n", addr, reg);
            exit(1);
        }
        if(read(fd, buf, 1) != 1){
            fprintf(stderr, "failed to read byte for addr %i and reg %i\n", addr, reg);
            exit(1);
        }
        value = buf[0];
    }else if(dir == GCORE_I2C_WRITE){
        buf[0] = reg;
        buf[1] = value;
        if(write(fd, buf, 2) != 2){
            fprintf(stderr, "failed to read byte for addr %i and reg %i\n", addr, reg);
            exit(1);
        }
    }else{
        die("Invalid direction given %i\n", dir);
    }

    close(fd);

    return value;
}

/*
 * Set the i2c switch to the given dev enum.
 */
void gcore_i2c_set_dev(enum gcore_i2c_devs dev){
    gcore_i2c(GCORE_I2C_SWITCH_ADDR, 0x00, GCORE_I2C_WRITE, dev);
}


