/*
 * prgm test programs
 *
 * Copyright (c) 2015-2021 Gemini Complex Corporation. All rights reserved.
 *
 */

#include "common.h"
#include "util.h"
#include "stim.h"
#include "board/artix.h"
#include "prgm.h"

#include "lib/fe/fe.h"
#include "lib/uthash/utarray.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <inttypes.h>
#include <ctype.h>
#include <setjmp.h>

#define BUFFER_SIZE (4096)

/*
 * Creates a prgm stim object to be added to the hashtable.
 *
 */
struct prgm_stim *_create_prgm_stim(struct stim *stim, uint32_t a1_addr, uint32_t a2_addr){
    struct prgm_stim *prgm_stim = NULL;
    if(stim == NULL){
        die("pointer is null");
    }
    if((prgm_stim = (struct prgm_stim *)malloc(sizeof(struct prgm_stim))) == NULL){
        die("failed to malloc");
    }

    prgm_stim->stim = stim;
    prgm_stim->a1_addr = a1_addr;
    prgm_stim->a2_addr = a2_addr;

    return prgm_stim;
}

enum load_types {
    LOAD,
    LOADS,
    LOADA
};

/*
 * Helper function that loads a stim into tester memory.
 *
 * LOAD = loads from profile/stim path at next available memory address
 * LOADS = loads from a stim object at next available memory address
 * LOADA = loads from a stim object and uses load address given
 *
 * TODO: if LOADA, check to make sure it doesn't overwrite another pattern
 * TODO: check if loading a stim will go out of memory bounds
 *
 */
static void _load_stim(fe_Context *_fe_ctx, fe_Object *arg, enum load_types load_type, 
        uint64_t *a1_addr, uint64_t *a2_addr, enum stim_modes *mode){
    char stim_path[BUFFER_SIZE];
    struct stim *stim = NULL;
    struct prgm *prgm = NULL;
    fe_Object *fe_stim_path = NULL;
    fe_Object *fe_stim = NULL;
    fe_Object *fe_prgm = NULL;
    fe_Object *fe_addr = NULL;
    uint32_t a1_load_addr = 0;
    uint32_t a2_load_addr = 0;
    struct prgm_stim *prgm_stim = NULL;
    struct prgm_stim *s;
    char buffer[BUFFER_SIZE];
    uint64_t num_loaded_bytes = 0;
    enum stim_modes stim_mode = STIM_MODE_NONE;

    if(a1_addr == NULL){
        die("pointer is null");
    }

    if(a2_addr == NULL){
        die("pointer is null");
    }

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    if(load_type == LOAD){
        if(prgm->_profile == NULL){
            fe_error(_fe_ctx, "failed to load stim because no board profile set");
        }

        fe_stim_path = fe_nextarg(_fe_ctx, &arg);
        fe_tostring(_fe_ctx, fe_stim_path, stim_path, sizeof(stim_path));

        if((stim = get_stim_by_path(prgm->_profile, stim_path)) == NULL){
            fe_error(_fe_ctx, "failed to load stim");
        }
    }else if(load_type == LOADS){
        fe_stim = fe_nextarg(_fe_ctx, &arg);
        if((stim = (struct stim*)fe_toptr(_fe_ctx, fe_stim)) == NULL){
            fe_error(_fe_ctx, "failed to get stim object");
        }
    }else if(load_type == LOADA){
        fe_stim = fe_nextarg(_fe_ctx, &arg);
        if((stim = (struct stim*)fe_toptr(_fe_ctx, fe_stim)) == NULL){
            fe_error(_fe_ctx, "failed to get stim object");
        }

        fe_addr = fe_nextarg(_fe_ctx, &arg);
        a1_load_addr = (uint32_t)fe_tonumber(_fe_ctx, fe_addr);
        a2_load_addr = (uint32_t)fe_tonumber(_fe_ctx, fe_addr);
    }

    stim_mode = stim_get_mode(stim);
    if(stim_mode == STIM_MODE_NONE){
        snprintf(buffer, BUFFER_SIZE, "can't load empty stim: '%s'", stim_path);
        fe_error(_fe_ctx, buffer);
    }

    if(load_type == LOAD || load_type == LOADS){
        // use a1 addr for both a1 and a2 if dual mode
        a1_load_addr = prgm->_cur_a1_stim_addr;
        a2_load_addr = prgm->_cur_a2_stim_addr;
    }

    if((prgm_stim = _create_prgm_stim(stim, a1_load_addr, a2_load_addr)) == NULL){
        die("failed to create prgm stim");
    }

    if(stim_mode == STIM_MODE_DUAL){
        HASH_FIND_INT(prgm->_a1_loaded_stims, &prgm_stim->a1_addr, s);
        if (s == NULL) {
            HASH_ADD_INT(prgm->_a1_loaded_stims, a1_addr, prgm_stim);
        }else{
            snprintf(buffer, BUFFER_SIZE, "stim already loaded at a1 address 0x%08X", a1_load_addr);
            fe_error(_fe_ctx, buffer);
        }

        HASH_FIND_INT(prgm->_a2_loaded_stims, &prgm_stim->a2_addr, s);
        if (s == NULL) {
            HASH_ADD_INT(prgm->_a2_loaded_stims, a2_addr, prgm_stim);
        }else{
            snprintf(buffer, BUFFER_SIZE, "stim already loaded at a2 address 0x%08X", a2_load_addr);
            fe_error(_fe_ctx, buffer);
        }

        num_loaded_bytes = artix_load_stim(stim, a1_load_addr, a2_load_addr);

        if(load_type == LOAD || load_type == LOADS){
            prgm->_cur_a1_stim_addr += num_loaded_bytes;
            prgm->_cur_a2_stim_addr += num_loaded_bytes;
        }
        prgm->_num_a1_loaded_stims += 1;
        prgm->_num_a2_loaded_stims += 1;
    }else if(stim_mode == STIM_MODE_A1){
        HASH_FIND_INT(prgm->_a1_loaded_stims, &prgm_stim->a1_addr, s);
        if (s == NULL) {
            HASH_ADD_INT(prgm->_a1_loaded_stims, a1_addr, prgm_stim);
        }else{
            snprintf(buffer, BUFFER_SIZE, "stim already loaded at a1 address 0x%08X", a1_load_addr);
            fe_error(_fe_ctx, buffer);
        }

        num_loaded_bytes = artix_load_stim(stim, a1_load_addr, a2_load_addr);
        if(load_type == LOAD || load_type == LOADS){
            prgm->_cur_a1_stim_addr += num_loaded_bytes;
        }
        prgm->_num_a1_loaded_stims += 1;
    }else if(stim_mode == STIM_MODE_A2){
        HASH_FIND_INT(prgm->_a2_loaded_stims, &prgm_stim->a2_addr, s);
        if (s == NULL) {
            HASH_ADD_INT(prgm->_a2_loaded_stims, a2_addr, prgm_stim);
        }else{
            snprintf(buffer, BUFFER_SIZE, "stim already loaded at a2 address 0x%08X", a2_load_addr);
            fe_error(_fe_ctx, buffer);
        }

        num_loaded_bytes = artix_load_stim(stim, a1_load_addr, a2_load_addr);
        if(load_type == LOAD || load_type == LOADS){
            prgm->_cur_a2_stim_addr += num_loaded_bytes;
        }
        prgm->_num_a2_loaded_stims += 1;
    }

    *a1_addr = a1_load_addr;
    *a2_addr = a2_load_addr;
    *mode = stim_mode;

    return;
}

static fe_Object * _run_stim(fe_Context *_fe_ctx, fe_Object *arg, bool run_continue){
    struct prgm *prgm = NULL;
    fe_Object *fe_addrs = NULL;
    fe_Object *fe_a1_addr = NULL;
    fe_Object *fe_a2_addr = NULL;
    fe_Object *fe_prgm = NULL;
    uint64_t a1_addr = 0;
    uint64_t a2_addr = 0;
    struct prgm_stim *a1_prgm_stim = NULL;
    struct prgm_stim *a2_prgm_stim = NULL;
    bool failed = false;
    bool did_test_fail = false;
    uint64_t test_cycle = 0;
    uint32_t num_tests_ran = 0;
    char buffer[BUFFER_SIZE];
    struct db_prgm *db_prgm = NULL;
    int64_t db_stim_id = -1;
    struct db_stim *db_stim = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    if(prgm->_db_prgm_id >= 0 && prgm->_db != NULL){
        if((db_prgm = db_get_prgm_by_id(prgm->_db, prgm->_db_prgm_id)) == NULL){
            die("failed to get db_prgm by id %lli", prgm->_db_prgm_id);
        }

        db_prgm->did_fail = 0;
        db_prgm->failing_vec = -1;
    }

    while(!fe_isnil(_fe_ctx, arg)){
        a1_addr = 0;
        a2_addr = 0;
        a1_prgm_stim = NULL;
        a2_prgm_stim = NULL;

        fe_addrs = fe_nextarg(_fe_ctx, &arg);
        fe_a1_addr = fe_car(_fe_ctx, fe_addrs);
        fe_a2_addr = fe_cdr(_fe_ctx, fe_addrs);

        if(fe_type(_fe_ctx, fe_a2_addr) == FE_TPAIR){
            fe_error(_fe_ctx, "failed to run because addrs given must be a cons pair not a list");
        }

        if(fe_isnil(_fe_ctx, fe_a1_addr) && fe_isnil(_fe_ctx, fe_a2_addr)){
            snprintf(buffer, BUFFER_SIZE, "failed to run stim because invalid address pair given");
            fe_error(_fe_ctx, buffer);
        }

        if(!fe_isnil(_fe_ctx, fe_a1_addr)){
            a1_addr = (uint32_t)fe_tonumber(_fe_ctx, fe_a1_addr);
            HASH_FIND_INT(prgm->_a1_loaded_stims, &a1_addr, a1_prgm_stim);
            if (a1_prgm_stim == NULL) {
                snprintf(buffer, BUFFER_SIZE, "no stim loaded at a1 address 0x%08" PRIX64 "", a1_addr);
                fe_error(_fe_ctx, buffer);
            }
        }

        if(!fe_isnil(_fe_ctx, fe_a2_addr)){
            a2_addr = (uint32_t)fe_tonumber(_fe_ctx, fe_a2_addr);
            HASH_FIND_INT(prgm->_a2_loaded_stims, &a2_addr, a2_prgm_stim);
            if (a2_prgm_stim == NULL) {
                snprintf(buffer, BUFFER_SIZE, "no stim loaded at a2 address 0x%08" PRIX64 "", a2_addr);
                fe_error(_fe_ctx, buffer);
            }
        }

        
        if(a1_prgm_stim == NULL && a2_prgm_stim == NULL){
            fe_error(_fe_ctx, "failed to run stim because no stim found at a1 addr or a2 addr");
        }

        if(prgm->_db_prgm_id >= 0 && prgm->_db != NULL){
            if(a1_prgm_stim != NULL){
                db_stim_id = db_insert_stim(prgm->_db, prgm->_db_prgm_id, 
                        a1_prgm_stim->stim->path, 0, -1, STIM_PENDING);
            }else if(a2_prgm_stim != NULL){
                db_stim_id = db_insert_stim(prgm->_db, prgm->_db_prgm_id, 
                        a2_prgm_stim->stim->path, 0, -1, STIM_PENDING);
            }
        }

        // Either stim loaded into a1, a2 or both. If both then it's a dual pattern and
        // the stim will be the same. It could be two solo patterns, but that's currently
        // not supported.
        // TODO: support running two loaded solo patterns.
        if(a1_prgm_stim != NULL && a2_prgm_stim != NULL){
            if((a1_prgm_stim != a2_prgm_stim) || (a1_prgm_stim->stim != a2_prgm_stim->stim)){
                snprintf(buffer, BUFFER_SIZE, "Failed to run because addr pair (0x%016" PRIX64 ", 0x%016" PRIX64 ") must be a dual stim loaded in both units.", a1_addr, a2_addr);
                fe_error(_fe_ctx, buffer);
            }

            failed = artix_run_stim(a1_prgm_stim->stim, &test_cycle, a1_addr, a2_addr);
            prgm->_last_prgm_stim = a1_prgm_stim;

        }else if(a1_prgm_stim != NULL){
            failed  = artix_run_stim(a1_prgm_stim->stim, &test_cycle, a1_prgm_stim->a1_addr, a1_prgm_stim->a2_addr);
            prgm->_last_prgm_stim = a1_prgm_stim;
        }else if(a2_prgm_stim != NULL){
            failed = artix_run_stim(a2_prgm_stim->stim, &test_cycle, a2_prgm_stim->a1_addr, a2_prgm_stim->a2_addr);
            prgm->_last_prgm_stim = a2_prgm_stim;
        }

        // save results to db
        if(db_stim_id >= 0 && db_prgm != NULL){
            if((db_stim = db_get_stim_by_id(prgm->_db, db_stim_id)) == NULL){
                die("failed to get db_stim by id %lli", db_stim_id);
            }
            db_stim->did_fail = (int32_t)failed;
            db_stim->failing_vec = (int64_t)test_cycle;
            db_stim->state = STIM_DONE;
            db_update_stim(prgm->_db, db_stim);

            db_prgm->last_stim_id = db_stim_id;
            db_prgm->did_fail = (int32_t)failed;
            db_prgm->failing_vec = (int64_t)test_cycle;
            db_update_prgm(prgm->_db, db_prgm);
        }

        num_tests_ran += 1;

        if(did_test_fail == false){
            did_test_fail = failed;
        }

        if(run_continue == false){
            if(did_test_fail == true){
                break;
            }
        }
    }

    fe_Object *ret[3];
    ret[0] = fe_number(_fe_ctx, num_tests_ran);
    ret[1] = fe_bool(_fe_ctx, did_test_fail);
    ret[2] = fe_number(_fe_ctx, test_cycle);
    return fe_list(_fe_ctx, ret, 3);
}

/*
 * (reads <stim_path:str>) -> <stim_object:ptr>
 *
 * Reads a stim file from disk and returns a stim object.
 *
 */
static fe_Object* f_reads(fe_Context *_fe_ctx, fe_Object *arg){
    char stim_path[BUFFER_SIZE];
    struct stim *stim = NULL;
    struct prgm *prgm = NULL;
    fe_Object *fe_prgm = NULL;
    fe_Object *fe_stim_path = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    if(prgm->_profile == NULL){
        fe_error(_fe_ctx, "failed to read stim because no board profile set");
    }

    fe_stim_path = fe_nextarg(_fe_ctx, &arg);
    fe_tostring(_fe_ctx, fe_stim_path, stim_path, sizeof(stim_path));

    if((stim = get_stim_by_path(prgm->_profile, stim_path)) == NULL){
        fe_error(_fe_ctx, "Failed to load stim.");
    }

    return fe_ptr(_fe_ctx, (void *) stim); 
}

/*
 * (writes <stim_object:ptr> <stim_path:str:) -> <stim_object:ptr>
 *
 * Write a stim object to disk at the given path string.
 *
 */
static fe_Object* f_writes(fe_Context *_fe_ctx, fe_Object *arg){
    struct stim *stim = NULL;
    char stim_path[BUFFER_SIZE];
    fe_Object *fe_stim = NULL;
    fe_Object *fe_stim_path = NULL;

    fe_stim = fe_nextarg(_fe_ctx, &arg);
    if((stim = (struct stim*)fe_toptr(_fe_ctx, fe_stim)) == NULL){
        fe_error(_fe_ctx, "failed to get stim object");
    }
    fe_stim_path = fe_nextarg(_fe_ctx, &arg);
    fe_tostring(_fe_ctx, fe_stim_path, stim_path, sizeof(stim_path));

    stim_serialize_to_path(stim, stim_path);

    return fe_ptr(_fe_ctx, (void *) stim); 
}

/*
 * (load <stim_path:str>) -> (<a1_addr:num>, <a2_addr:num>)
 *
 * Sequentially loads the test pattern stim into tester memory after the
 * previously loaded test pattern. Returns the address in tester memory
 * where the stim was loaded.
 *
 */
static fe_Object* f_load(fe_Context *_fe_ctx, fe_Object *arg){
    uint64_t a1_load_addr = 0;
    uint64_t a2_load_addr = 0;
    enum stim_modes stim_mode = STIM_MODE_NONE;
    _load_stim(_fe_ctx, arg, LOAD, &a1_load_addr, &a2_load_addr, &stim_mode);

    if(stim_mode == STIM_MODE_NONE){
        die("stim mode is none");
    }else if(stim_mode == STIM_MODE_DUAL){
        return fe_cons(_fe_ctx, fe_number(_fe_ctx, a1_load_addr), fe_number(_fe_ctx, a2_load_addr));
    }else if(stim_mode == STIM_MODE_A1){
        return fe_cons(_fe_ctx, fe_number(_fe_ctx, a1_load_addr), fe_bool(_fe_ctx, false));
    }else if(stim_mode == STIM_MODE_A2){
        return fe_cons(_fe_ctx, fe_bool(_fe_ctx, false), fe_number(_fe_ctx, a2_load_addr));
    }else{
        die("invalid stim mode");
    }
}

/*
 * (loads <stim_object>) -> (<a1_addr>, <a2_addr>)
 *
 * Sequentially loads the test pattern stim into tester memory after the
 * previously loaded test pattern given a stim object. Returns the address in
 * tester memory where the stim was loaded.
 *
 */
static fe_Object* f_loads(fe_Context *_fe_ctx, fe_Object *arg){
    uint64_t a1_load_addr = 0;
    uint64_t a2_load_addr = 0;
    enum stim_modes stim_mode = STIM_MODE_NONE;
    _load_stim(_fe_ctx, arg, LOADS, &a1_load_addr, &a2_load_addr, &stim_mode);

    if(stim_mode == STIM_MODE_NONE){
        die("stim mode is none");
    }else if(stim_mode == STIM_MODE_DUAL){
        return fe_cons(_fe_ctx, fe_number(_fe_ctx, a1_load_addr), fe_number(_fe_ctx, a2_load_addr));
    }else if(stim_mode == STIM_MODE_A1){
        return fe_cons(_fe_ctx, fe_number(_fe_ctx, a1_load_addr), fe_bool(_fe_ctx, false));
    }else if(stim_mode == STIM_MODE_A2){
        return fe_cons(_fe_ctx, fe_bool(_fe_ctx, false), fe_number(_fe_ctx, a2_load_addr));
    }else{
        die("invalid stim mode");
    }
}

/*
 * (loada <stim_object> (<a1_addr>, <a2_addr>)) -> (<a1_addr>, <a2_addr>)
 *
 * Loads the stim object given to the tester memory at the address given.
 * Returns the address in tester memory where the stim was loaded.
 *
 * DANGER: this can and will overwrite tester memory
 *
 */
static fe_Object* f_loada(fe_Context *_fe_ctx, fe_Object *arg){
    uint64_t a1_load_addr = 0;
    uint64_t a2_load_addr = 0;
    enum stim_modes stim_mode = STIM_MODE_NONE;
    _load_stim(_fe_ctx, arg, LOADA, &a1_load_addr, &a2_load_addr, &stim_mode);

    if(stim_mode == STIM_MODE_NONE){
        die("stim mode is none");
    }else if(stim_mode == STIM_MODE_DUAL){
        return fe_cons(_fe_ctx, fe_number(_fe_ctx, a1_load_addr), fe_number(_fe_ctx, a2_load_addr));
    }else if(stim_mode == STIM_MODE_A1){
        return fe_cons(_fe_ctx, fe_number(_fe_ctx, a1_load_addr), fe_bool(_fe_ctx, false));
    }else if(stim_mode == STIM_MODE_A2){
        return fe_cons(_fe_ctx, fe_bool(_fe_ctx, false), fe_number(_fe_ctx, a2_load_addr));
    }else{
        die("invalid stim mode");
    }
}

/*
 * (unload (<a1_addr>, <a2_addr)) -> nil
 *
 * Unloads a stim at the address pair given.
 *
 */
static fe_Object* f_unload(fe_Context *_fe_ctx, fe_Object *arg){
    struct prgm *prgm = NULL;
    fe_Object *fe_addrs = NULL;
    fe_Object *fe_a1_addr = NULL;
    fe_Object *fe_a2_addr = NULL;
    fe_Object *fe_prgm = NULL;
    uint32_t a1_addr = 0;
    uint32_t a2_addr = 0;
    struct prgm_stim *a1_prgm_stim = NULL;
    struct prgm_stim *a2_prgm_stim = NULL;
    char buffer[BUFFER_SIZE];

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    fe_addrs = fe_nextarg(_fe_ctx, &arg);

    fe_a1_addr = fe_car(_fe_ctx, fe_addrs);
    fe_a2_addr = fe_cdr(_fe_ctx, fe_addrs);

    if(fe_type(_fe_ctx, fe_a2_addr) == FE_TPAIR){
        fe_error(_fe_ctx, "failed to unload because addrs given must be a cons pair not a list");
    }

    if(!fe_isnil(_fe_ctx, fe_a1_addr)){
        a1_addr = (uint32_t)fe_tonumber(_fe_ctx, fe_a1_addr);
        HASH_FIND_INT(prgm->_a1_loaded_stims, &a1_addr, a1_prgm_stim);
        if (a1_prgm_stim == NULL) {
            snprintf(buffer, BUFFER_SIZE, "no stim loaded at a1 address 0x%08X", a1_addr);
            fe_error(_fe_ctx, buffer);
        }
        prgm->_num_a1_loaded_stims -= 1;
        HASH_DEL(prgm->_a1_loaded_stims, a1_prgm_stim);
    }

    if(!fe_isnil(_fe_ctx, fe_a2_addr)){
        a2_addr = (uint32_t)fe_tonumber(_fe_ctx, fe_a2_addr);
        HASH_FIND_INT(prgm->_a2_loaded_stims, &a2_addr, a2_prgm_stim);
        if (a2_prgm_stim == NULL) {
            snprintf(buffer, BUFFER_SIZE, "no stim loaded at a2 address 0x%08X", a2_addr);
            fe_error(_fe_ctx, buffer);
        }
        prgm->_num_a2_loaded_stims -= 1;
        HASH_DEL(prgm->_a2_loaded_stims, a2_prgm_stim);
    }

    if(a1_prgm_stim != NULL && a2_prgm_stim != NULL){
        if(a1_prgm_stim == a2_prgm_stim){
            free_stim(a1_prgm_stim->stim);
            free(a1_prgm_stim);
        }else{
            if(a1_prgm_stim != NULL){
                free_stim(a1_prgm_stim->stim);
                free(a1_prgm_stim);
            }
            if(a2_prgm_stim != NULL){
                free_stim(a2_prgm_stim->stim);
                free(a2_prgm_stim);
            }
        }
    }else{
        if(a1_prgm_stim != NULL){
            free_stim(a1_prgm_stim->stim);
            free(a1_prgm_stim);
        }
        if(a2_prgm_stim != NULL){
            free_stim(a2_prgm_stim->stim);
            free(a2_prgm_stim);
        }
    }

    return fe_bool(_fe_ctx, false); 
}

/*
 * (unload-all) -> (<num_a1_unloaded_stims>, <num_a2_unloaded_stims>)
 *
 * Unloads all stims and returns num stims that were unloaded. Returns the
 * number of stims that were unloaded.
 *
 *
 */
static fe_Object* f_unload_all(fe_Context *_fe_ctx, fe_Object *arg){
    fe_Object *fe_prgm = NULL;
    struct prgm *prgm = NULL;
    struct prgm_stim *prgm_stim = NULL;
    struct prgm_stim *prgm_stim_tmp = NULL;
    uint64_t num_a1_unloaded_stims = 0;
    uint64_t num_a2_unloaded_stims = 0;

    struct del_prgm_stims {
        struct prgm_stim *prgm_stim;
        UT_hash_handle hh;
    };
    struct del_prgm_stims *del_prgm_stims = NULL;
    struct del_prgm_stims *del_prgm_stim = NULL;
    struct del_prgm_stims *del_prgm_stim_tmp = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    HASH_ITER(hh, prgm->_a1_loaded_stims, prgm_stim, prgm_stim_tmp) {
        HASH_DEL(prgm->_a1_loaded_stims, prgm_stim);
        prgm->_num_a1_loaded_stims -= 1;
        num_a1_unloaded_stims += 1;

        HASH_FIND_INT(del_prgm_stims, &prgm_stim, del_prgm_stim);
        if(del_prgm_stim == NULL){
            if((del_prgm_stim = (struct del_prgm_stims*)malloc(sizeof(struct del_prgm_stims))) == NULL){
                die("malloc failed");
            }
            del_prgm_stim->prgm_stim = prgm_stim;
            HASH_ADD_INT(del_prgm_stims, prgm_stim, del_prgm_stim);
        }
    }
    prgm->_cur_a1_stim_addr = 0;

    HASH_ITER(hh, prgm->_a2_loaded_stims, prgm_stim, prgm_stim_tmp) {
        HASH_DEL(prgm->_a2_loaded_stims, prgm_stim);
        prgm->_num_a2_loaded_stims -= 1;
        num_a2_unloaded_stims += 1;

        HASH_FIND_INT(del_prgm_stims, &prgm_stim, del_prgm_stim);
        if(del_prgm_stim == NULL){
            if((del_prgm_stim = (struct del_prgm_stims*)malloc(sizeof(struct del_prgm_stims))) == NULL){
                die("malloc failed");
            }
            del_prgm_stim->prgm_stim = prgm_stim;
            HASH_ADD_INT(del_prgm_stims, prgm_stim, del_prgm_stim);
        }
    }
    prgm->_cur_a2_stim_addr = 0;

    // only free prgm_stims once
    HASH_ITER(hh, del_prgm_stims, del_prgm_stim, del_prgm_stim_tmp) {
        free_stim(del_prgm_stim->prgm_stim->stim);
        free(del_prgm_stim->prgm_stim);
        free(del_prgm_stim);
    }

    return fe_cons(_fe_ctx, fe_number(_fe_ctx, num_a1_unloaded_stims), fe_number(_fe_ctx, num_a2_unloaded_stims));
}

/*
 * (run <addr1> <addr2> ...) -> (<num_tests_ran>, <did_test_fail>, <fail_test_cycle>)
 *
 * Executes loaded stims at the tester memory addresses given. Stops at the
 * first failing pattern.
 *
 */
static fe_Object* f_run(fe_Context *_fe_ctx, fe_Object *arg){
    bool run_continue = false;
    return _run_stim(_fe_ctx, arg, run_continue);
}

/*
 * (runc <addr1> <addr2> ...) -> (<num_tests_ran>, <did_test_fail>, <fail_test_cycle>)
 *
 * Executes loaded stims at the tester memory addresses given. Will execute all
 * stims without stopping if it fails.
 *
 */
static fe_Object* f_runc(fe_Context *_fe_ctx, fe_Object *arg){
    bool run_continue = true;
    return _run_stim(_fe_ctx, arg, run_continue);
}

/*
 * (set-profile "board_profile.json") -> nil
 *
 * Globally sets the board profile to be used for subsequent
 * "load" calls. Returns nil because the profile is saved globally.
 *
 */
static fe_Object* f_set_profile(fe_Context *_fe_ctx, fe_Object *arg){
    fe_Object *fe_prgm = NULL;
    fe_Object *fe_profile_path = NULL;
    char profile_path[4096];
    struct prgm *prgm = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    fe_profile_path = fe_nextarg(_fe_ctx, &arg);
    fe_tostring(_fe_ctx, fe_profile_path, profile_path, sizeof(profile_path));

    if((prgm->_profile = get_profile_by_path(profile_path)) == NULL){
        die("error: pointer is NULL");
    }

    return fe_bool(_fe_ctx, false); 
}

/*
 * (get-pin-names) -> (<p0:str>, <p1:str>, ...)
 *
 * Returns a list of pin string net names for the last stim
 * that ran.
 *
 * If no stim ran, returns nil;
 *
 */
static fe_Object* f_get_pin_names(fe_Context *_fe_ctx, fe_Object *arg){
    uint32_t num_fail_pins = 0;
    uint8_t *fail_pins = NULL;
    struct prgm *prgm = NULL;
    struct stim *stim = NULL;
    fe_Object *fe_prgm = NULL;
    struct profile_pin *pin = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    if(prgm->_last_prgm_stim == NULL){
        return fe_bool(_fe_ctx, false);
    }

    if((stim = prgm->_last_prgm_stim->stim) == NULL){
        die("failed to get stim");
    }

    artix_get_stim_fail_pins(&fail_pins, &num_fail_pins);

    fe_Object **ret = NULL;

    if((ret = (fe_Object **)malloc(sizeof(fe_Object*)*stim->num_pins)) == NULL){
        die("malloc failed");
    }

    for(int i=0; i<stim->num_pins; i++){
        pin = stim->pins[i];
        ret[i] = fe_string(_fe_ctx, strdup(pin->net_name));
    }
    free(fail_pins);

    return fe_list(_fe_ctx, ret, stim->num_pins);
}


/*
 * (get-fail-pins) -> (<p0:bool>, <p1:bool>, ...)
 *
 * Returns the failing pins for the last stim that ran. The failing pins is a
 * list that corresponds to the Pin line in the dots. If the bool is true, that
 * pin failed. Call (pin-names) to return a list of the pin names.
 *
 * If no stim ran, returns nil;
 *
 */
static fe_Object* f_get_fail_pins(fe_Context *_fe_ctx, fe_Object *arg){
    uint32_t num_fail_pins = 0;
    uint8_t *fail_pins = NULL;
    struct prgm *prgm = NULL;
    struct stim *stim = NULL;
    fe_Object *fe_prgm = NULL;
    struct profile_pin *pin = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    if(prgm->_last_prgm_stim == NULL){
        return fe_bool(_fe_ctx, false);
    }

    if((stim = prgm->_last_prgm_stim->stim) == NULL){
        die("failed to get stim");
    }

    artix_get_stim_fail_pins(&fail_pins, &num_fail_pins);

    fe_Object **ret = NULL;

    if((ret = (fe_Object **)malloc(sizeof(fe_Object*)*stim->num_pins)) == NULL){
        die("malloc failed");
    }

    for(int i=0; i<stim->num_pins; i++){
        pin = stim->pins[i];
        if(fail_pins[pin->dut_io_id]){
            ret[i] = fe_bool(_fe_ctx, true);
        }else{
            ret[i] = fe_bool(_fe_ctx, false);
        }
    }
    free(fail_pins);

    return fe_list(_fe_ctx, ret, stim->num_pins);
}

/*
 * (exit <code:num>) -> nil
 *
 * Exits with return code.
 *
 */
static fe_Object* f_exit(fe_Context *_fe_ctx, fe_Object *arg){
    fe_Object *fe_ret = NULL;
    uint32_t ret = 0;
    struct prgm *prgm = NULL;
    fe_Object *fe_prgm = NULL;

    fe_prgm = fe_eval(_fe_ctx, fe_symbol(_fe_ctx, "prgm"));
    if((prgm = (struct prgm*)fe_toptr(_fe_ctx, fe_prgm)) == NULL){
        fe_error(_fe_ctx, "failed to get global prgm object");
    }

    fe_ret = fe_nextarg(_fe_ctx, &arg);
    if(fe_isnil(_fe_ctx, fe_ret)){
        fe_error(_fe_ctx, "must give an exit code number");
    }
    ret = (uint32_t)fe_tonumber(_fe_ctx, fe_ret);

    prgm_close(prgm);

    exit(ret);
    return fe_bool(_fe_ctx, false);
}

/*
 * Adds the gemini cfuncs to fe global funcion list.
 *
 */
void _add_fe_gemini_funcs(fe_Context *_fe_ctx){
    if(_fe_ctx == NULL){
        die("ptr is NULL");
    }
    
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "load"), fe_cfunc(_fe_ctx, f_load)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "reads"), fe_cfunc(_fe_ctx, f_reads)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "writes"), fe_cfunc(_fe_ctx, f_writes));
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "loads"), fe_cfunc(_fe_ctx, f_loads)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "loada"), fe_cfunc(_fe_ctx, f_loada)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "unload"), fe_cfunc(_fe_ctx, f_unload)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "unload-all"), fe_cfunc(_fe_ctx, f_unload_all)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "run"), fe_cfunc(_fe_ctx, f_run)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "runc"), fe_cfunc(_fe_ctx, f_runc)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "set-profile"), fe_cfunc(_fe_ctx, f_set_profile)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "get-pin-names"), fe_cfunc(_fe_ctx, f_get_pin_names)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "get-fail-pins"), fe_cfunc(_fe_ctx, f_get_fail_pins)); 
    fe_set(_fe_ctx, fe_symbol(_fe_ctx, "exit"), fe_cfunc(_fe_ctx, f_exit)); 
}


/*
 * Creates a new fe context and sets the global 'prgm' symbol to the prgm
 * pointer.
 *
 */
void _prgm_create_fe_ctx(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(!(prgm->_fe_data == NULL && prgm->_fe_ctx == NULL)){
        die("fe ctx already created");
    }
    
    prgm->_fe_data_size = FE_DATA_SIZE;

    if((prgm->_fe_data = malloc(prgm->_fe_data_size)) == NULL){
        die("failed to malloc fe data");
    }

    if((prgm->_fe_ctx = fe_open(prgm->_fe_data, prgm->_fe_data_size)) == NULL){
        die("failed to create fe context");
    }

    fe_set(prgm->_fe_ctx, fe_symbol(prgm->_fe_ctx, "prgm"), fe_ptr(prgm->_fe_ctx, (void *)prgm)); 

    return;
}

/*
 * Frees the fe context for the prgm object.
 *
 */
void _prgm_free_fe_ctx(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(prgm->_fe_data == NULL && prgm->_fe_ctx == NULL){
        die("fe ctx already freed");
    }

    if(prgm->_fe_ctx != NULL){
        fe_close(prgm->_fe_ctx);
        prgm->_fe_ctx = NULL;
    }

    if(prgm->_fe_data != NULL){
        free(prgm->_fe_data);
        prgm->_fe_data = NULL;
    }

    prgm->_fe_data_size = 0;

    return;
}

struct prgm *prgm_create(){
    struct prgm *prgm = NULL;

    if((prgm = (struct prgm*)malloc(sizeof(struct prgm))) == NULL){
        die("failed to malloc prgm");
    }

    prgm->_path_fd = 0;
    prgm->_path_fp = NULL;
    prgm->_path_size = 0;
    prgm->_db_prgm_id = -1;

    prgm->_db = NULL;

    prgm->_fe_data_size = 0;
    prgm->_fe_data = NULL;
    prgm->_fe_ctx = NULL;

    _prgm_create_fe_ctx(prgm);
    _add_fe_gemini_funcs(prgm->_fe_ctx);

    prgm->_profile = NULL;
    prgm->_num_a1_loaded_stims = 0;
    prgm->_num_a2_loaded_stims = 0;
    prgm->_cur_a1_stim_addr = 0;
    prgm->_cur_a2_stim_addr = 0;
    prgm->_a1_loaded_stims = NULL;
    prgm->_a2_loaded_stims = NULL;
    prgm->_last_prgm_stim = NULL;

    return prgm;
}

/*
 * Open a prgm at path for running. If db_prgm_id is set to id for row in db,
 * will write results and log output to db at db_path given. Otherwise it must
 * be set to -1 and db_path set to NULL;
 *
 */
void prgm_open(struct prgm *prgm, const char *path, int64_t db_prgm_id, const char *db_path){
    char *real_path = NULL;

    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(path == NULL){
        die("pointer is NULL");
    }

    if(prgm->is_path_open){
        die("prgm path '%s' already open", path);
    }

    if((real_path = realpath(path, NULL)) == NULL){
        die("invalid prgm path '%s' because %s", path, strerror(errno));
    }

    prgm->path = strdup(real_path);
    free(real_path);

    if(db_prgm_id < 0){
        if(db_prgm_id != -1){
            die("db_prgm_id given '%lli' must be -1 if less than zero", db_prgm_id);
        }
        prgm->_db_prgm_id = db_prgm_id;
    }else if(db_prgm_id >= 0){
        prgm->_db_prgm_id = db_prgm_id;
        if(db_path == NULL){
            die("db_prgm_id '%lli' given, but db_path is null", db_prgm_id);
        }
        prgm->_db_path = strdup(db_path);
        if((prgm->_db = db_create()) == NULL){
            die("failed to create db");
        }
        db_open(prgm->_db, prgm->_db_path);
        if(db_get_prgm_by_id(prgm->_db, prgm->_db_prgm_id) == NULL){
            die("db_prgm_id '%lli' given, but row not found in db", db_prgm_id);
        }
    }

    if(util_fopen(prgm->path, &prgm->_path_fd, &prgm->_path_fp, &prgm->_path_size)){
        die("failed to open prgm file '%s'", path);
    }

    prgm->is_path_open = true;

    return;
}

void prgm_close(struct prgm *prgm){
    if(prgm->is_path_open){
        fclose(prgm->_path_fp);
        close(prgm->_path_fd);
        if(prgm->_db != NULL){
            db_close(prgm->_db);
        }
        prgm->_path_size = 0;
        prgm->is_path_open = false;
    }
    return;
}

void prgm_free(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(prgm->path != NULL){
        free(prgm->path);
    }

    if(prgm->_db_path != NULL){
        free((char *)prgm->_db_path);
    }

    if(prgm->_db != NULL){
        db_free(prgm->_db);
    }

    prgm_close(prgm);

    _prgm_free_fe_ctx(prgm);

    if(prgm->_profile != NULL){
        free_profile(prgm->_profile);
    }

    // TODO: free loaded stims

    free(prgm);
    return;
}


int prgm_run(struct prgm *prgm){
    if(prgm == NULL){
        die("pointer is NULL");
    }

    if(prgm->is_path_open == false){
        die("prgm is not open");
    }

    int ret = prgm_repl(prgm, prgm->_path_fp, stdout, stderr);

    // rewind prgm path if we want to re-run
    fseek(prgm->_path_fp, 0, SEEK_SET);

    return ret;
}

static jmp_buf fe_jmp_buf;
static FILE *fe_fp_err = NULL;

static void _fe_onerror(fe_Context *ctx, const char *msg, fe_Object *cl) {
    //unused(ctx), unused(cl);
    FILE *fp_err = stderr;
    if(fe_fp_err != NULL){
        fp_err = fe_fp_err;
    }
    fprintf(fp_err, "error: %s\n", msg);
    longjmp(fe_jmp_buf, -1);
}

int prgm_repl(struct prgm *prgm, FILE *fp_in, FILE *fp_out, FILE *fp_err){
    int gc;
    fe_Object *obj;

    if(prgm == NULL){
        die("pointer is NULL");
    }

    FILE *volatile _fp_in = stdin;
    FILE  *_fp_out = stdout;

    if(fp_in != NULL){
        _fp_in = fp_in;
    }

    if(fp_out != NULL){
        _fp_out = fp_out;
    }

    if(fp_err != NULL){
        fe_fp_err = fp_err;
    }else{
        fe_fp_err = stderr;
    }

    if(_fp_in == stdin){ 
        fe_handlers(prgm->_fe_ctx)->error = _fe_onerror;
    }

    gc = fe_savegc(prgm->_fe_ctx);
    int jmp_ret = setjmp(fe_jmp_buf);

    if(jmp_ret == -1){
        return EXIT_FAILURE;
    }

    // bust out the repl
    for(;;){
        fe_restoregc(prgm->_fe_ctx, gc);
        if(_fp_in == stdin){ fprintf(_fp_out, "> "); }

        obj = fe_readfp(prgm->_fe_ctx, _fp_in);

        // nothing left to read
        if(!obj){ break; }

        // evaluate read object
        obj = fe_eval(prgm->_fe_ctx, obj);

        if(_fp_in == stdin) {
            fe_writefp(prgm->_fe_ctx, obj, _fp_out);
            fprintf(_fp_out, "\n");
        }

    }

    return EXIT_SUCCESS;
}




