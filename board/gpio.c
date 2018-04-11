/*
 * gpio helper functions
 *
 */


// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "../common.h"
#include "gpio.h"

#include "../../driver/gcore_common.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

/* ==========================================================================
 * SYSFS GPIO API 
 * ==========================================================================
 */

bool gcore_gpio(uint32_t index, enum gcore_gpio_dir dir, bool value){
    if((index < GCORE_GPIO_MIN) || (GCORE_GPIO_MAX > GCORE_GPIO_MAX)){
        die("gpio idx given %i is out of bounds (> 118)", index);
    }

    // the 0th gpio starts at /sys/class/gpio/gpiochip<value> up to 1023
    // for zynq and 511 for zynqMP.
    index = index + GCORE_GPIO_START; 
    
    int32_t fd = 0;
    if((fd = open("/sys/class/gpio/export", O_WRONLY)) < 0){
        die("failed to open gpio %i for export", index);
    }

    char *pin_str = NULL;
    if((pin_str = (char*)calloc(50, sizeof(char))) == NULL){
        die("pointer is NULL\n");
    }
    snprintf(pin_str, 50, "%d", index);

    // exporting pin
    write(fd, pin_str, strlen(pin_str)+1); 
    close(fd);

    char *pin_path = NULL;
    if((pin_path = (char*)calloc(50, sizeof(char))) == NULL){
        die("pointer is NULL");
    }

    // set direction to out
    snprintf(pin_path, 50, "/sys/class/gpio/gpio%s/direction", pin_str);
    if((fd = open(pin_path, O_RDWR)) < 0){
        die("failed to open direction path %s for r/w", pin_path);
    }

    if(dir == GCORE_GPIO_READ){
        write(fd, "in", 3);
        close(fd);
    }else if(dir == GCORE_GPIO_WRITE){
        write(fd, "out", 4);
        close(fd);
    }

    // set to given value
    snprintf(pin_path, 50, "/sys/class/gpio/gpio%s/value", pin_str);
    if((fd = open(pin_path, O_RDWR)) < 0){
        die("failed to open value path %s for r/w", pin_path);
    }

    if(dir == GCORE_GPIO_READ){
        char *read_str = NULL;
        if((read_str = (char*)calloc(1, sizeof(char))) == NULL){
            die("pointer is NULL");
        }
        if(read(fd, read_str, 1) < 0){
            die("failed to read gpio %s", pin_str);
        }
        if(read_str[0] == '0'){
            value = false;
        }else if(read_str[0] == '1'){
            value = true;
        }else{
            die("invalid value read %s. Not '0' or '1'", read_str);
        }
        free(read_str);
    }else if(dir == GCORE_GPIO_WRITE){
        if(value == false){
            write(fd, "0", 2);
        }else{
            write(fd, "1", 2);
        }
    }else{
        die("invalid direction given %i", dir);
    }

    close(fd);
    free(pin_path);

    //if((fd = open("/sys/class/gpio/unexport", O_WRONLY)) ){
    //    die("failed to open gpio %i for unexport", index);
    //}

    // exporting pin
    //write(fd, pin_str, strlen(pin_str)+1); 
    //close(fd);
    free(pin_str);

    return value;
}

