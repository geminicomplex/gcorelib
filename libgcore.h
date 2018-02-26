#ifndef LIBGCORE_H
#define LIBGCORE_H

// support for files larger than 2GB limit
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../driver/gemini_core.h"

#include "lib/progress/progressbar.h"
#include "lib/progress/statusbar.h"

#include "artix.h"
#include "config.h"
#include "dma.h"
#include "dots.h"
#include "gpio.h"
#include "helper.h"
#include "i2c.h"
#include "profile.h"
#include "stim.h"
#include "subcore.h"
#include "util.h"



#ifdef __cplusplus
}
#endif
#endif

