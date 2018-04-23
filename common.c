/*
 * Common
 *
 */

// support for files larger than 2GB limit
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "common.h"
#include <stdio.h>

static bool did_init = false;

__attribute__((constructor))
static void gcore_init() {
    gcore_init_log(GCORE_LOG_PATH);
    return;
}

/*
 * Logging defaults to stderr. If called, will write log to
 * path given.
 *
 */
void gcore_init_log(const char *log_path){
    if(!did_init){
        slog_init(log_path, 1, 3, 1, 1, 1);
        did_init = true;
    }
    return;
}














