/*
 * Dev provides access to /dev/gcore
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef DEV_H
#define DEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver.h"


uint8_t gcore_dev_get_fd();
uint8_t *gcore_dev_get_map();

#ifdef __cplusplus
}
#endif
#endif /* DEV_H */
