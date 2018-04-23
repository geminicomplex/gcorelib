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
        char template[] = "/tmp/slogXXXXXX";
        char *conf_path = NULL;
        if((conf_path = mktemp(template)) == NULL){
            return;
        }
        FILE *fp = fopen(conf_path, "w"); 
        fprintf(fp, "LOGTOFILE 1\n");
        fprintf(fp, "PRETTYLOG 1\n");
        fprintf(fp, "FILESTAMP 1\n");
        fclose(fp);
        slog_init(log_path, conf_path, 1, 3, 1);
        did_init = true;
    }
    return;
}














