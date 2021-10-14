/*
 * GPIO helper functions
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#ifndef GPIO_H
#define GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver.h"

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

#ifdef __cplusplus
}
#endif
#endif  /* GPIO_H */
