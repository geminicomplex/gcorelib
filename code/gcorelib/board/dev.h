#ifndef DEV_H
#define DEV_H

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


uint8_t gcore_dev_get_fd();
uint8_t *gcore_dev_get_map();

#ifdef __cplusplus
}
#endif
#endif				/* DEV_H */
