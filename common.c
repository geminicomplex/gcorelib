/*
 * Common
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#include "common.h"
#include <stdio.h>
#include <execinfo.h>
#include <unistd.h>

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


// helper-function to print the current stack trace
void print_stacktrace(){
    void *buffer[MAX_STACK_LEVELS];
    int levels = backtrace(buffer, MAX_STACK_LEVELS);
    fprintf(stderr, "----------------------------------------------------------------------\n");
    backtrace_symbols_fd(buffer + 1, levels - 1, STDERR_FILENO);
    fprintf(stderr, "----------------------------------------------------------------------\n");
}











