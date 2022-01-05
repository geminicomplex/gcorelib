/*
 * prgm test programs
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */
#ifndef PRGM_H
#define PRGM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lib/uthash/uthash.h"
#include "lib/fe/fe.h"

// 32 MB fe data scratch pad
#define FE_DATA_SIZE (1024*1024*3)


/*
 * Represents a loaded stim in tester memory.
 */
struct prgm_stim {
    uint64_t a1_addr;
    uint64_t a2_addr;
    struct stim *stim;
    UT_hash_handle hh;
};


/*
 * A test program object.
 */
struct prgm {
    
    /*
     * public
     */
    char *path;
    bool is_path_open;

    /* 
     * private
     */

    // prgm path
    int _path_fd;
    FILE *_path_fp;
    off_t _path_size;

    // fe context
    uint32_t _fe_data_size;
    void *_fe_data;
    fe_Context *_fe_ctx;

    // stim
    struct profile *_profile;
    uint64_t _num_a1_loaded_stims;
    uint64_t _num_a2_loaded_stims;
    uint64_t _cur_a1_stim_addr;
    uint64_t _cur_a2_stim_addr;
    struct prgm_stim *_a1_loaded_stims;
    struct prgm_stim *_a2_loaded_stims;
    struct prgm_stim *_last_prgm_stim;
};


struct prgm *prgm_create();
void prgm_open(struct prgm *prgm, char *path);
void prgm_close(struct prgm *prgm);
void prgm_free(struct prgm *prgm);
int prgm_run(struct prgm *prgm);
int prgm_repl(struct prgm *prgm, FILE *fp_in, FILE *fp_out, FILE *fp_err);


#ifdef __cplusplus
}
#endif
#endif
