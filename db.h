/*
 * db.h - database api
 *
 * Copyright (c) 2015-2022 Gemini Complex Corporation. All rights reserved.
 *
 */
#ifndef DB_H
#define DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sqlite3.h"

enum db_job_states {
    JOB_NONE     = (1 << 0), 
    JOB_IDLE     = (1 << 1), 
    JOB_PENDING  = (1 << 2), 
    JOB_RUNNING  = (1 << 3), 
    JOB_KILLING  = (1 << 4), 
    JOB_KILLED   = (1 << 5), 
    JOB_DONE     = (1 << 6)
};

enum db_prgm_states {
    PRGM_NONE     = (1 << 0), 
    PRGM_IDLE     = (1 << 1), 
    PRGM_PENDING  = (1 << 2), 
    PRGM_RUNNING  = (1 << 3), 
    PRGM_KILLING  = (1 << 4), 
    PRGM_KILLED   = (1 << 5), 
    PRGM_DONE     = (1 << 6)
};

enum db_stim_states {
    STIM_NONE     = (1 << 0), 
    STIM_IDLE     = (1 << 1), 
    STIM_PENDING  = (1 << 2), 
    STIM_RUNNING  = (1 << 3), 
    STIM_KILLING  = (1 << 4), 
    STIM_KILLED   = (1 << 5), 
    STIM_DONE     = (1 << 6)
};

struct db_user {
    int64_t id;
    const char *username;
    const char *password;
    const char *session;
    int32_t is_admin;
};

struct db_board {
    int64_t id;
    const char *dna;
    const char *name;
    const char *ip_addr;
    int32_t cur_dut_board_id;
    int32_t is_master;
};

struct db_dut_board {
    int64_t id;
    const char *dna;
    const char *name;
    const char *profile_path;
};

struct db_job {
    int64_t id;
    int64_t board_id;
    int64_t dut_board_id;
    int64_t user_id;
    enum db_job_states state;
};

struct db_prgm {
    int64_t id;
    int64_t job_id;
    const char *path;
    const char *body;
    time_t date_start;
    time_t date_end;
    int32_t return_code;
    const char *error_msg;
    int64_t last_stim_id;
    int32_t did_fail;
    int64_t failing_vec;
    enum db_prgm_states state;
};

struct db_prgm_log {
    int64_t id;
    int64_t prgm_id;
    time_t date_created;
    const char *line;
};

struct db_stim {
    int64_t id;
    int64_t prgm_id;
    const char *path;
    int32_t did_fail;
    int64_t failing_vec;
    enum db_stim_states state;
};

struct db_fail_pin {
    int64_t id;
    int64_t stim_id;
    int64_t dut_io_id;
    int32_t did_fail;
};

struct db_mount {
    int64_t id;
    const char *name;
    const char *ip_addr;
    const char *path;
    const char *point;
    const char *message;
};

struct db {

    /*
     * public
     */
    char *path;
    bool is_open;

    /*
     * private
     */
    sqlite3 *_db;

};


/*
 * db management funcs
 */
struct db *db_create();
void db_open(struct db *db, char *path);
void db_close(struct db *db);
void db_free(struct db *db);

/*
 * Use these to free structs returned from
 * funcs below.
 *
 */
void db_free_user();
void db_free_board();
void db_free_dut_board();
void db_free_job();
void db_free_prgm();
void db_free_stim();
void db_free_fail_pin();
void db_free_mount();

/*
 * use these to insert, update or get rows from db
 */
int64_t db_insert_board(struct db *db, 
        char *dna, char *name, char *ip_addr, int32_t cur_dut_board_id, int32_t is_master);
int64_t db_update_board(struct db *db, struct db_board *board);
struct db_board* db_get_board_by_id(struct db *db, int64_t board_id);
struct db_board* db_get_board_by_dna(struct db *db, char *dna);
int64_t db_update_job(struct db *db, struct db_job *job);
uint64_t db_get_num_jobs(struct db *db, 
        int64_t board_id, int64_t dut_board_id,
        int64_t user_id, int32_t states);
struct db_job** db_get_jobs(struct db *db, 
        int64_t board_id, int64_t dut_board_id, 
        int64_t user_id, int32_t states);
struct db_prgm* db_get_prgm_by_id(struct db *db, int64_t prgm_id);
int64_t db_update_prgm(struct db *db, struct db_prgm *prgm);
uint64_t db_get_num_prgms(struct db *db, 
        int64_t job_id, const char *path, const char *body, 
        int32_t return_code, int32_t last_stim_id, 
        int32_t did_fail, int32_t failing_vec, int32_t states);
struct db_prgm** db_get_prgms(struct db *db, 
        int64_t job_id, const char *path, const char *body,
        int32_t return_code, int32_t last_stim_id,
        int32_t did_fail, int32_t failing_vec, int32_t states);
int64_t db_insert_prgm_log(struct db *db, 
        int64_t prgm_id, char *line);
int64_t db_insert_stim(struct db *db, 
        int64_t prgm_id, char *path, int32_t did_fail, 
        int64_t failing_vec, enum db_stim_states state);
int64_t db_update_stim(struct db *db, struct db_stim *stim);

#ifdef __cplusplus
}
#endif
#endif
