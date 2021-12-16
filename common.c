/*
 * Common
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <execinfo.h>
#include <unistd.h>

#include "common.h"

static bool did_init = false;

/*
 * Enables all log levels including: SLOG_LIVE, SLOG_DEBUG, SLOG_TRACE
 */
void gcore_enable_log_debug(){
    int enabled_levels = SLOG_FLAGS_ALL;
    slog_config_t slgCfg;
    slog_config_get(&slgCfg);
    slgCfg.nFlags = enabled_levels;
    slog_config_set(&slgCfg);
}

/*
 * Enables all log levels except: SLOG_LIVE, SLOG_DEBUG, SLOG_TRACE
 */
void gcore_disable_log_debug(){
    int enabled_levels = SLOG_FATAL | SLOG_ERROR | SLOG_WARN | SLOG_INFO | SLOG_NOTAG;
    slog_config_t slgCfg;
    slog_config_get(&slgCfg);
    slgCfg.nFlags = enabled_levels;
    slog_config_set(&slgCfg);
}

/*
 * Logging defaults to stderr. If called, will write log to
 * path given.
 *
 */
void gcore_init(){
    if(!did_init){
        int enabled_levels = SLOG_FATAL | SLOG_ERROR | SLOG_WARN | SLOG_INFO | SLOG_NOTAG;
        slog_init(GCORE_LOG_NAME, enabled_levels, 1);
        slog_config_t slgCfg;
        slog_config_get(&slgCfg);
        strcpy(slgCfg.sFilePath, GCORE_LOG_DIR);
        slgCfg.nToScreen = 1;
        slgCfg.nToFile = 1;
        slgCfg.nFlush = 1;
        slog_config_set(&slgCfg);
        did_init = true;
    }
    return;
}

static void _gcore_destroy(){
    slog_destroy();
    return;
}

__attribute__((constructor))
static void gcore_constructor() {
    gcore_init();
    return;
}

__attribute__((destructor))
static void gcore_destructor() {
    _gcore_destroy();
    return;
}


// helper-function to print the current stack trace
void print_stacktrace(){
    void *buffer[MAX_STACK_LEVELS];
    int levels = backtrace(buffer, MAX_STACK_LEVELS);
    fprintf(stderr, "----------------------------------------------------------------------\n");
    backtrace_symbols_fd(buffer + 1, levels - 1, STDERR_FILENO);
    fprintf(stderr, "----------------------------------------------------------------------\n");
}











