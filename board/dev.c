/*
 * dev provides access to /dev/gcore 
 *
 */


// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "../common.h"
#include "dev.h"

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


static int gcore_fd = -1;
static uint8_t *gcore_map = NULL;

__attribute__((constructor))
static void gcore_dev_init() {

    gcore_init_log(GCORE_LOG_PATH);

#ifdef __arm__
    gcore_fd = open(MMAP_PATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
	if(gcore_fd == -1){
		die("gcorelib: opening file for writing");
    }
	
    gcore_map = mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, gcore_fd, 0);
	if(gcore_map == MAP_FAILED){
		close(gcore_fd);
		die("gcorelib: mmapping the file");
    }

#endif
    return;
}

__attribute__((destructor))
static void gcore_dev_destroy() {

    gcore_init_log(GCORE_LOG_PATH);

#ifdef __arm__
	if(munmap(gcore_map, MMAP_SIZE) == -1){
		die("gcorelib: error un-mmapping the file");
	}
	close(gcore_fd);

    
#endif
	return;
}


uint8_t gcore_dev_get_fd(){
    return gcore_fd;
}

uint8_t *gcore_dev_get_map(){
    return gcore_map;
}


